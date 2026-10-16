// Reading SVG into an sc::drawing, and writing one out as SVG.

#include "drawing.h"

#include <tinyxml2.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <map>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string>

namespace sc {
    namespace {
        // ---------------------------------------------------------------- reading: small parsers

        bool is_space(const char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ','; }

        // Numbers one after the other, as SVG writes them: "1,2 3-4.5.5" is 1, 2, 3, -4.5, 0.5.
        class numbers {
        public:
            explicit numbers(const char *text) : at_(text ? text : "") {
            }

            bool next(double &value) {
                skip();
                if (!*at_ || std::isalpha(static_cast<unsigned char>(*at_)) && *at_ != 'e' && *at_ != 'E') return false;
                char *end = nullptr;
                value = std::strtod(at_, &end);
                if (end == at_) return false;
                at_ = end;
                return true;
            }

            // An arc's flag: one 0 or 1, which may run straight into what follows ("a5 5 0 01 10 10").
            bool flag(bool &value) {
                skip();
                if (*at_ != '0' && *at_ != '1') return false;
                value = *at_++ == '1';
                return true;
            }

            // The next command letter, if that's what comes next.
            bool command(char &letter) {
                skip();
                if (!std::isalpha(static_cast<unsigned char>(*at_)) || *at_ == 'e' || *at_ == 'E') return false;
                letter = *at_++;
                return true;
            }

            [[nodiscard]] bool done() {
                skip();
                return !*at_;
            }

        private:
            void skip() {
                while (*at_ && is_space(*at_)) ++at_;
            }

            const char *at_;
        };

        double number(const char *text, const double fallback = 0) {
            double value = fallback;
            numbers{text}.next(value);
            return value;
        }

        std::string trimmed(const std::string &text);

        // A length in pixels (CSS's 96 per inch): a number with px, pt, pc, mm, cm or in; 0 for a
        // percentage or nothing, which the viewBox then decides.
        double length(const char *text) {
            if (!text) return 0;
            char *end = nullptr;
            const double value = std::strtod(text, &end);
            if (end == text) return 0;
            const std::string unit = trimmed(end);
            if (unit.empty() || unit == "px") return value;
            if (unit == "pt") return value * 96 / 72;
            if (unit == "pc") return value * 16;
            if (unit == "mm") return value * 96 / 25.4;
            if (unit == "cm") return value * 96 / 2.54;
            if (unit == "in") return value * 96;
            return 0; // %, em and the like
        }

        double angle_between(const double ux, const double uy, const double vx, const double vy) {
            return std::atan2(ux * vy - uy * vx, ux * vx + uy * vy);
        }

        // An elliptical arc as cubic curves (SVG's endpoint form, as its spec converts it).
        void arc_to(path &shape, const point &from, double rx, double ry, const double rotation, const bool large, const bool sweep, const point &to) {
            if (from == to) return;
            rx = std::abs(rx);
            ry = std::abs(ry);
            if (rx == 0 || ry == 0) {
                shape.line_to(to);
                return;
            }
            const double phi = rotation * std::numbers::pi / 180, cos_phi = std::cos(phi), sin_phi = std::sin(phi);
            const double dx = (from.x() - to.x()) / 2, dy = (from.y() - to.y()) / 2;
            const double x1 = cos_phi * dx + sin_phi * dy, y1 = -sin_phi * dx + cos_phi * dy;
            if (const double reach = x1 * x1 / (rx * rx) + y1 * y1 / (ry * ry); reach > 1) { // radii too small: scaled up to reach
                rx *= std::sqrt(reach);
                ry *= std::sqrt(reach);
            }
            const double numerator = rx * rx * ry * ry - rx * rx * y1 * y1 - ry * ry * x1 * x1;
            const double denominator = rx * rx * y1 * y1 + ry * ry * x1 * x1;
            const double factor = (large == sweep ? -1 : 1) * std::sqrt(std::max(0.0, numerator / denominator));
            const double cx1 = factor * rx * y1 / ry, cy1 = -factor * ry * x1 / rx;
            const double cx = cos_phi * cx1 - sin_phi * cy1 + (from.x() + to.x()) / 2;
            const double cy = sin_phi * cx1 + cos_phi * cy1 + (from.y() + to.y()) / 2;
            const double start = angle_between(1, 0, (x1 - cx1) / rx, (y1 - cy1) / ry);
            double sweep_angle = angle_between((x1 - cx1) / rx, (y1 - cy1) / ry, (-x1 - cx1) / rx, (-y1 - cy1) / ry);
            if (!sweep && sweep_angle > 0) sweep_angle -= 2 * std::numbers::pi;
            if (sweep && sweep_angle < 0) sweep_angle += 2 * std::numbers::pi;

            // At most a quarter turn per curve.
            const int pieces = std::max(1, static_cast<int>(std::ceil(std::abs(sweep_angle) / (std::numbers::pi / 2) - 1e-9)));
            const double step = sweep_angle / pieces, k = 4.0 / 3 * std::tan(step / 4);
            const auto on = [&](const double t) {
                return point{cx + rx * std::cos(t) * cos_phi - ry * std::sin(t) * sin_phi, cy + rx * std::cos(t) * sin_phi + ry * std::sin(t) * cos_phi};
            };
            const auto along = [&](const double t) {
                return point{-rx * std::sin(t) * cos_phi - ry * std::cos(t) * sin_phi, -rx * std::sin(t) * sin_phi + ry * std::cos(t) * cos_phi};
            };
            for (int i = 0; i < pieces; ++i) {
                const double t1 = start + i * step, t2 = t1 + step;
                const point end = i == pieces - 1 ? to : on(t2);
                shape.cubic_to(on(t1) + along(t1) * k, end - along(t2) * k, end);
            }
        }

        // SVG path data: every command, relative ones and the shorthands included.
        path path_data(const char *text) {
            path shape;
            numbers in{text};
            point current, start, last_control;
            char command = 0, previous = 0;
            while (!in.done()) {
                if (char letter = 0; in.command(letter)) command = letter;
                else if (!command) break; // numbers before any command: not path data
                const bool relative = std::islower(static_cast<unsigned char>(command));
                const point base = relative ? current : point{};
                double x = 0, y = 0, x1 = 0, y1 = 0, x2 = 0, y2 = 0;
                const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(command)));
                switch (upper) {
                    case 'M':
                        if (!in.next(x) || !in.next(y)) return shape;
                        current = start = base + point{x, y};
                        shape.move_to(current);
                        command = relative ? 'l' : 'L'; // pairs after a move are lines
                        break;
                    case 'L':
                        if (!in.next(x) || !in.next(y)) return shape;
                        current = base + point{x, y};
                        shape.line_to(current);
                        break;
                    case 'H':
                        if (!in.next(x)) return shape;
                        current = {relative ? current.x() + x : x, current.y()};
                        shape.line_to(current);
                        break;
                    case 'V':
                        if (!in.next(y)) return shape;
                        current = {current.x(), relative ? current.y() + y : y};
                        shape.line_to(current);
                        break;
                    case 'C':
                        if (!in.next(x1) || !in.next(y1) || !in.next(x2) || !in.next(y2) || !in.next(x) || !in.next(y)) return shape;
                        last_control = base + point{x2, y2};
                        shape.cubic_to(base + point{x1, y1}, last_control, base + point{x, y});
                        current = base + point{x, y};
                        break;
                    case 'S': {
                        if (!in.next(x2) || !in.next(y2) || !in.next(x) || !in.next(y)) return shape;
                        const char before = static_cast<char>(std::toupper(static_cast<unsigned char>(previous)));
                        const point first = before == 'C' || before == 'S' ? current * 2 - last_control : current;
                        last_control = base + point{x2, y2};
                        shape.cubic_to(first, last_control, base + point{x, y});
                        current = base + point{x, y};
                        break;
                    }
                    case 'Q':
                    case 'T': {
                        point control;
                        if (upper == 'Q') {
                            if (!in.next(x1) || !in.next(y1)) return shape;
                            control = base + point{x1, y1};
                        } else {
                            const char before = static_cast<char>(std::toupper(static_cast<unsigned char>(previous)));
                            control = before == 'Q' || before == 'T' ? current * 2 - last_control : current;
                        }
                        if (!in.next(x) || !in.next(y)) return shape;
                        const point end = base + point{x, y};
                        shape.cubic_to(current + (control - current) * (2.0 / 3), end + (control - end) * (2.0 / 3), end); // as a cubic
                        last_control = control;
                        current = end;
                        break;
                    }
                    case 'A': {
                        double rx = 0, ry = 0, rotation = 0;
                        bool large = false, sweep = false;
                        if (!in.next(rx) || !in.next(ry) || !in.next(rotation) || !in.flag(large) || !in.flag(sweep) || !in.next(x) || !in.next(y))
                            return shape;
                        const point end = base + point{x, y};
                        arc_to(shape, current, rx, ry, rotation, large, sweep, end);
                        current = end;
                        break;
                    }
                    case 'Z':
                        shape.close();
                        current = start;
                        break;
                    default:
                        return shape; // not a path command
                }
                previous = command;
                if (upper == 'Z') command = 0; // a number after Z needs a new command
            }
            return shape;
        }

        // A transform attribute: matrix, translate, scale, rotate, skewX and skewY, applied right to left.
        transform transform_list(const char *text) {
            transform combined;
            if (!text) return combined;
            const std::string list{text};
            std::size_t at = 0;
            while (true) {
                const auto open = list.find('(', at);
                if (open == std::string::npos) break;
                const auto close = list.find(')', open);
                if (close == std::string::npos) break;
                std::string name = list.substr(at, open - at);
                name.erase(std::remove_if(name.begin(), name.end(), [](const char c) { return is_space(c); }), name.end());
                const std::string arguments = list.substr(open + 1, close - open - 1);
                std::vector<double> v;
                numbers in{arguments.c_str()};
                for (double value = 0; in.next(value);) v.push_back(value);
                const auto arg = [&](const std::size_t i, const double fallback = 0) { return i < v.size() ? v[i] : fallback; };
                transform step;
                if (name == "matrix" && v.size() == 6) step = transform{v[0], v[1], v[2], v[3], v[4], v[5]};
                else if (name == "translate") step = transform::translate(arg(0), arg(1));
                else if (name == "scale") step = transform::scale(arg(0, 1), arg(1, arg(0, 1)));
                else if (name == "rotate") step = transform::rotate(arg(0), {arg(1), arg(2)});
                else if (name == "skewX") step = transform{1, 0, std::tan(arg(0) * std::numbers::pi / 180), 1, 0, 0};
                else if (name == "skewY") step = transform{1, std::tan(arg(0) * std::numbers::pi / 180), 0, 1, 0, 0};
                combined = step.then(combined); // the rightmost applies first
                at = close + 1;
            }
            return combined;
        }

        std::string trimmed(const std::string &text) {
            const auto first = text.find_first_not_of(" \t\n\r");
            if (first == std::string::npos) return {};
            return text.substr(first, text.find_last_not_of(" \t\n\r") - first + 1);
        }

        // The element's name without a namespace prefix ("svg:rect" is "rect").
        std::string local_name(const tinyxml2::XMLElement &element) {
            const std::string name = element.Name();
            const auto colon = name.find(':');
            return colon == std::string::npos ? name : name.substr(colon + 1);
        }

        // What "#id" or "url(#id)" points at.
        std::string reference(const std::string &value) {
            const auto hash = value.find('#');
            if (hash == std::string::npos) return {};
            auto id = value.substr(hash + 1);
            if (const auto end = id.find_first_of(")\"' "); end != std::string::npos) id.erase(end);
            return id;
        }

        // ---------------------------------------------------------------- reading: the document

        // The paint and placement in effect for an element: what it inherits, plus its own.
        struct context {
            std::optional<color> fill = color{0, 0, 0};
            fill_rule rule = fill_rule::nonzero;
            std::optional<color> stroke;
            double stroke_width = 1;
            double fill_opacity = 1, stroke_opacity = 1, opacity = 1;
            font lettering{"sans-serif", 16}; // SVG's default size
            std::string anchor = "start";
            transform placed; // from the element's own coordinates to the drawing's
            std::optional<path> clip;
        };

        class reader {
        public:
            explicit reader(drawing &into) : into_(into) {
            }

            void read(const tinyxml2::XMLElement &root) {
                index(root);
                context start;
                const char *view_box = root.Attribute("viewBox");
                std::vector<double> box;
                numbers in{view_box};
                for (double value = 0; in.next(value);) box.push_back(value);
                const double width = length(root.Attribute("width")), height = length(root.Attribute("height"));
                if (box.size() == 4 && box[2] > 0 && box[3] > 0) {
                    // The viewBox at the origin, fitted into the width and height when they're given
                    // (scaled evenly and centred, as SVG's preserveAspectRatio does by default).
                    const double page_width = width > 0 ? width : height > 0 ? box[2] * height / box[3] : box[2];
                    const double page_height = height > 0 ? height : width > 0 ? box[3] * width / box[2] : box[3];
                    const double scale = std::min(page_width / box[2], page_height / box[3]);
                    into_.page({page_width, page_height});
                    start.placed = transform::translate(-box[0], -box[1])
                                       .then(transform::scale(scale))
                                       .then(transform::translate((page_width - box[2] * scale) / 2, (page_height - box[3] * scale) / 2));
                } else {
                    into_.page({width, height});
                }
                apply_paint(root, start);
                children(root, start);
            }

        private:
            drawing &into_;
            std::map<std::string, const tinyxml2::XMLElement *> ids_;
            int depth_ = 0; // of <use>, against references that loop

            void index(const tinyxml2::XMLElement &element) {
                if (const char *id = element.Attribute("id")) ids_.emplace(id, &element);
                for (auto *child = element.FirstChildElement(); child; child = child->NextSiblingElement()) index(*child);
            }

            // A property: from style="..." first (it wins), then the presentation attribute.
            static std::optional<std::string> property(const tinyxml2::XMLElement &element, const std::string &name) {
                if (const char *style = element.Attribute("style")) {
                    std::istringstream declarations{style};
                    for (std::string declaration; std::getline(declarations, declaration, ';');) {
                        const auto colon = declaration.find(':');
                        if (colon != std::string::npos && trimmed(declaration.substr(0, colon)) == name) return trimmed(declaration.substr(colon + 1));
                    }
                }
                if (const char *value = element.Attribute(name.c_str())) return trimmed(value);
                return std::nullopt;
            }

            static std::optional<color> paint(const std::string &value, const std::optional<color> &inherited) {
                if (value == "none" || value == "transparent") return std::nullopt;
                if (value == "inherit") return inherited;
                if (value == "currentColor") return color{0, 0, 0};
                if (value.starts_with("url(")) { // a gradient or pattern: its fallback colour, if any
                    const auto after = trimmed(value.substr(value.find(')') + 1));
                    if (after.empty()) return std::nullopt;
                    return paint(after, inherited);
                }
                try {
                    return color::from_string(value);
                } catch (const std::exception &) {
                    return inherited;
                }
            }

            void apply_paint(const tinyxml2::XMLElement &element, context &now) const {
                if (const auto v = property(element, "fill")) now.fill = paint(*v, now.fill);
                if (const auto v = property(element, "stroke")) now.stroke = paint(*v, now.stroke);
                if (const auto v = property(element, "fill-rule")) now.rule = *v == "evenodd" ? fill_rule::even_odd : fill_rule::nonzero;
                if (const auto v = property(element, "stroke-width")) now.stroke_width = number(v->c_str(), now.stroke_width);
                if (const auto v = property(element, "fill-opacity")) now.fill_opacity = number(v->c_str(), 1);
                if (const auto v = property(element, "stroke-opacity")) now.stroke_opacity = number(v->c_str(), 1);
                if (const auto v = property(element, "opacity")) now.opacity *= number(v->c_str(), 1);
                if (const auto v = property(element, "font-family")) {
                    auto family = trimmed(v->substr(0, v->find(','))); // the first of a list
                    family.erase(std::remove(family.begin(), family.end(), '\''), family.end());
                    family.erase(std::remove(family.begin(), family.end(), '"'), family.end());
                    if (!family.empty()) now.lettering.family = family;
                }
                if (const auto v = property(element, "font-size")) now.lettering.size = number(v->c_str(), now.lettering.size);
                if (const auto v = property(element, "font-weight")) {
                    if (*v == "bold") now.lettering.weight = 700;
                    else if (*v == "normal") now.lettering.weight = 400;
                    else now.lettering.weight = static_cast<int>(number(v->c_str(), now.lettering.weight));
                }
                if (const auto v = property(element, "font-style")) now.lettering.italic = *v == "italic" || *v == "oblique";
                if (const auto v = property(element, "text-anchor")) now.anchor = *v;
                now.placed = transform_list(element.Attribute("transform")).then(now.placed);
                if (const auto v = property(element, "clip-path"); v && *v != "none") {
                    if (const auto found = ids_.find(reference(*v)); found != ids_.end()) now.clip = clip_path(*found->second, now.placed);
                } else if (const auto mask = property(element, "mask"); mask && *mask != "none") {
                    if (const auto found = ids_.find(reference(*mask)); found != ids_.end()) now.clip = clip_path(*found->second, now.placed);
                }
            }

            static color with_opacity(const color &colour, const double opacity) {
                if (opacity >= 1) return colour; // as it is: rebuilding it from its channels could round it
                return {colour.red(), colour.green(), colour.blue(), colour.alpha() * std::clamp(opacity, 0.0, 1.0)};
            }

            [[nodiscard]] appearance look(const context &now, const bool fills = true) const {
                const double scale = now.placed.scale_factor();
                return {now.stroke ? sc::stroke{with_opacity(*now.stroke, now.stroke_opacity * now.opacity), now.stroke_width * scale} : sc::stroke::none(),
                        fills && now.fill ? sc::fill{with_opacity(*now.fill, now.fill_opacity * now.opacity), now.rule} : sc::fill::none()};
            }

            // A basic shape or path in the element's own coordinates; nothing for anything else.
            static std::optional<shape> geometry_of(const tinyxml2::XMLElement &element, const std::string &name) {
                const auto at = [&](const char *attribute, const double fallback = 0) { return number(element.Attribute(attribute), fallback); };
                if (name == "rect") {
                    const rect box{at("x"), at("y"), at("width"), at("height")};
                    if (box.width() <= 0 || box.height() <= 0) return std::nullopt;
                    const double radius = element.Attribute("rx") ? at("rx") : at("ry"); // rx and ry the same; an ellipse's corners aren't kept
                    if (radius > 0) return shape{rounded_rect{box, radius}};
                    return shape{box};
                }
                if (name == "circle") {
                    if (at("r") <= 0) return std::nullopt;
                    return shape{circle{{at("cx"), at("cy")}, at("r")}};
                }
                if (name == "ellipse") {
                    const double rx = at("rx"), ry = at("ry");
                    if (rx <= 0 || ry <= 0) return std::nullopt;
                    const point centre{at("cx"), at("cy")};
                    return shape{transform::scale(1, ry / rx, centre).apply(to_path(circle{centre, rx}))};
                }
                if (name == "line") return shape{line{{at("x1"), at("y1")}, {at("x2"), at("y2")}}};
                if (name == "polygon" || name == "polyline") {
                    std::vector<point> corners;
                    numbers in{element.Attribute("points")};
                    for (double x = 0, y = 0; in.next(x) && in.next(y);) corners.emplace_back(x, y);
                    if (corners.size() < 2) return std::nullopt;
                    if (name == "polygon") return shape{polygon{std::move(corners)}};
                    path open;
                    for (const auto &corner: corners) open.line_to(corner);
                    return shape{open};
                }
                if (name == "path") {
                    auto data = path_data(element.Attribute("d"));
                    if (data.empty()) return std::nullopt;
                    return shape{std::move(data)};
                }
                return std::nullopt;
            }

            // A clipPath's (or mask's) shapes as one path in the drawing's coordinates. A mask is taken
            // as a clip of its shapes, their luminance aside: that's what icon exports (Sketch's) use
            // masks for.
            std::optional<path> clip_path(const tinyxml2::XMLElement &clip, const transform &placed) const {
                path outline;
                collect(clip, transform_list(clip.Attribute("transform")).then(placed), outline, 0);
                if (outline.empty()) return std::nullopt;
                return outline;
            }

            // Every shape under parent, through <g> and <use>, into outline.
            void collect(const tinyxml2::XMLElement &parent, const transform &placed, path &outline, const int depth) const {
                if (depth > 16) return;
                for (auto *child = parent.FirstChildElement(); child; child = child->NextSiblingElement()) {
                    const std::string name = local_name(*child);
                    const transform own = transform_list(child->Attribute("transform")).then(placed);
                    if (name == "g") {
                        collect(*child, own, outline, depth + 1);
                    } else if (name == "use") {
                        const auto *href = child->Attribute("href") ? child->Attribute("href") : child->Attribute("xlink:href");
                        const auto found = href ? ids_.find(reference(href)) : ids_.end();
                        if (found == ids_.end()) continue;
                        const transform moved = transform::translate(number(child->Attribute("x")), number(child->Attribute("y"))).then(own);
                        if (const auto shape = geometry_of(*found->second, local_name(*found->second)))
                            outline.append(to_path(transform_list(found->second->Attribute("transform")).then(moved).apply(*shape)));
                        else
                            collect(*found->second, transform_list(found->second->Attribute("transform")).then(moved), outline, depth + 1);
                    } else if (const auto shape = geometry_of(*child, name)) {
                        outline.append(to_path(own.apply(*shape)));
                    }
                }
            }

            void children(const tinyxml2::XMLElement &parent, const context &now) {
                for (auto *child = parent.FirstChildElement(); child; child = child->NextSiblingElement()) element(*child, now);
            }

            static std::string text_of(const tinyxml2::XMLNode &node) {
                std::string all;
                for (auto *child = node.FirstChild(); child; child = child->NextSibling()) {
                    if (const auto *text = child->ToText()) all += text->Value();
                    else if (child->ToElement()) all += text_of(*child);
                }
                // Runs of white space as one space, as SVG renders them.
                std::string collapsed;
                for (const char c: all) {
                    const bool blank = c == ' ' || c == '\n' || c == '\t' || c == '\r';
                    if (blank && (collapsed.empty() || collapsed.back() == ' ')) continue;
                    collapsed += blank ? ' ' : c;
                }
                return trimmed(collapsed);
            }

            void element(const tinyxml2::XMLElement &node, const context &inherited) {
                const std::string name = local_name(node);
                if (name == "defs" || name == "clipPath" || name == "symbol" || name == "title" || name == "desc" || name == "metadata" ||
                    name == "style" || name == "linearGradient" || name == "radialGradient" || name == "pattern" || name == "marker" ||
                    name == "mask" || name == "filter")
                    return; // not drawn where they are
                context now = inherited;
                if (name == "use") {
                    const auto *href = node.Attribute("href") ? node.Attribute("href") : node.Attribute("xlink:href");
                    const auto found = href ? ids_.find(reference(href)) : ids_.end();
                    if (found == ids_.end() || depth_ > 16) return;
                    now.placed = transform::translate(number(node.Attribute("x")), number(node.Attribute("y"))).then(now.placed);
                    apply_paint(node, now);
                    ++depth_;
                    if (local_name(*found->second) == "symbol") children(*found->second, now);
                    else element(*found->second, now);
                    --depth_;
                    return;
                }
                if (name == "svg") now.placed = transform::translate(number(node.Attribute("x")), number(node.Attribute("y"))).then(now.placed);
                apply_paint(node, now);
                if (name == "g" || name == "svg" || name == "a" || name == "switch") {
                    children(node, now);
                    return;
                }
                if (name == "text") {
                    if (!now.fill) return;
                    auto content = text_of(node);
                    if (content.empty()) return;
                    font lettering = now.lettering;
                    lettering.size *= now.placed.scale_factor();
                    text sample{std::move(content), now.placed.apply(point{number(node.Attribute("x")), number(node.Attribute("y"))}), lettering};
                    if (now.anchor == "middle" || now.anchor == "end") {
                        const double width = label{sample}.bounds().width();
                        sample.position.x(sample.position.x() - (now.anchor == "middle" ? width / 2 : width));
                    }
                    into_.add(std::move(sample), sc::fill{with_opacity(*now.fill, now.fill_opacity * now.opacity)}, now.clip);
                    return;
                }
                if (const auto found = geometry_of(node, name)) {
                    const auto appearance = look(now, name != "line");
                    if (appearance.visible()) into_.add(now.placed.apply(*found), appearance, now.clip);
                }
            }
        };

        // ---------------------------------------------------------------- writing

        std::string format(const double value) {
            std::ostringstream out;
            out << std::setprecision(10) << (std::abs(value) < 1e-10 ? 0.0 : value);
            return out.str();
        }

        std::string format(const point &p) { return format(p.x()) + " " + format(p.y()); }

        std::string escaped(const std::string &text) {
            std::string out;
            for (const char c: text) {
                switch (c) {
                    case '&': out += "&amp;"; break;
                    case '<': out += "&lt;"; break;
                    case '>': out += "&gt;"; break;
                    case '"': out += "&quot;"; break;
                    default: out += c;
                }
            }
            return out;
        }

        std::string attribute(const std::string &name, const std::string &value) { return " " + name + "=\"" + value + "\""; }

        std::string path_d(const path &shape) {
            std::string d;
            for (const auto &step: shape.steps()) {
                if (!d.empty()) d += ' ';
                switch (step.type) {
                    case path::kind::move: d += "M" + format(step.to); break;
                    case path::kind::line: d += "L" + format(step.to); break;
                    case path::kind::cubic: d += "C" + format(step.first) + " " + format(step.second) + " " + format(step.to); break;
                    case path::kind::close: d += "Z"; break;
                }
            }
            return d;
        }

        std::string colour_attributes(const std::string &name, const color &colour) {
            std::string out = attribute(name, colour.to_hex());
            if (colour.alpha() < 1) out += attribute(name + "-opacity", format(colour.alpha()));
            return out;
        }

        std::string paint_attributes(const appearance &look) {
            std::string out;
            if (look.fill().visible()) {
                out += colour_attributes("fill", look.fill().colour());
                if (look.fill().rule() == fill_rule::even_odd) out += attribute("fill-rule", "evenodd");
            } else {
                out += attribute("fill", "none");
            }
            if (look.stroke().visible()) {
                out += colour_attributes("stroke", look.stroke().colour());
                out += attribute("stroke-width", format(look.stroke().width()));
            }
            return out;
        }

        // One shape as an element, with extra (its paint and clip) on it.
        std::string element_of(const shape &held, const std::string &extra) {
            return std::visit([&]<typename S>(const S &each) -> std::string {
                if constexpr (std::same_as<S, rect>) {
                    return "<rect" + attribute("x", format(each.left())) + attribute("y", format(each.top())) + attribute("width", format(each.width())) +
                           attribute("height", format(each.height())) + extra + "/>";
                } else if constexpr (std::same_as<S, rounded_rect>) {
                    const rect box = each.rect();
                    return "<rect" + attribute("x", format(box.left())) + attribute("y", format(box.top())) + attribute("width", format(box.width())) +
                           attribute("height", format(box.height())) + attribute("rx", format(each.radius())) + attribute("ry", format(each.radius())) + extra + "/>";
                } else if constexpr (std::same_as<S, rotated_rect>) {
                    const point c = each.center();
                    return "<rect" + attribute("x", format(c.x() - each.width() / 2)) + attribute("y", format(c.y() - each.height() / 2)) +
                           attribute("width", format(each.width())) + attribute("height", format(each.height())) +
                           attribute("transform", "rotate(" + format(each.angle()) + " " + format(c) + ")") + extra + "/>";
                } else if constexpr (std::same_as<S, polygon>) {
                    std::string points;
                    for (const auto &corner: each) points += (points.empty() ? "" : " ") + format(corner.x()) + "," + format(corner.y());
                    return "<polygon" + attribute("points", points) + extra + "/>";
                } else if constexpr (std::same_as<S, circle>) {
                    return "<circle" + attribute("cx", format(each.center().x())) + attribute("cy", format(each.center().y())) +
                           attribute("r", format(each.radius())) + extra + "/>";
                } else if constexpr (std::same_as<S, line>) {
                    return "<line" + attribute("x1", format(each.from().x())) + attribute("y1", format(each.from().y())) + attribute("x2", format(each.to().x())) +
                           attribute("y2", format(each.to().y())) + extra + "/>";
                } else if constexpr (std::same_as<S, path>) {
                    return "<path" + attribute("d", path_d(each)) + extra + "/>";
                } else {
                    std::string inside;
                    for (const auto &item: each) inside += element_of(item, "");
                    return "<g" + extra + ">" + inside + "</g>";
                }
            }, held.value());
        }
    }

    drawing drawing::from_svg(const std::string_view svg) {
        tinyxml2::XMLDocument document;
        if (document.Parse(svg.data(), svg.size()) != tinyxml2::XML_SUCCESS) throw std::runtime_error("sc::drawing: not SVG (bad XML)");
        const auto *root = document.RootElement();
        if (!root || local_name(*root) != "svg") throw std::runtime_error("sc::drawing: not SVG (the root isn't <svg>)");
        drawing result;
        reader{result}.read(*root);
        return result;
    }

    drawing drawing::load_svg(const std::filesystem::path &file) {
        std::ifstream in{file, std::ios::binary};
        if (!in) throw std::runtime_error("sc::drawing: can't read " + file.string());
        const std::string text{std::istreambuf_iterator<char>{in}, {}};
        return from_svg(text);
    }

    std::string drawing::to_svg() const {
        // Without a page, one from the origin to what's drawn.
        sc::size frame = page_;
        if (frame.area() <= 0) {
            const rect covered = bounds();
            frame = {std::max(0.0, covered.right()), std::max(0.0, covered.bottom())};
        }
        std::vector<path> clips; // each written once, as clip1, clip2, ...
        const auto clip_attribute = [&](const std::optional<path> &clip) -> std::string {
            if (!clip) return {};
            auto found = std::find(clips.begin(), clips.end(), *clip);
            if (found == clips.end()) found = clips.insert(clips.end(), *clip);
            return attribute("clip-path", "url(#clip" + std::to_string(found - clips.begin() + 1) + ")");
        };
        std::string body;
        for (const auto &item: items_) {
            if (const auto *shape = std::get_if<drawable>(&item)) {
                body += "  " + element_of(shape->shape, paint_attributes(shape->appearance) + clip_attribute(shape->clip)) + "\n";
            } else {
                const auto &text = std::get<label>(item);
                const auto &spec = text.text.font;
                body += "  <text" + attribute("x", format(text.text.position.x())) + attribute("y", format(text.text.position.y())) +
                        attribute("font-family", escaped(spec.family)) + attribute("font-size", format(spec.size)) +
                        (spec.weight != 400 ? attribute("font-weight", std::to_string(spec.weight)) : "") +
                        (spec.italic ? attribute("font-style", "italic") : "") + colour_attributes("fill", text.fill.colour()) +
                        clip_attribute(text.clip) + ">" + escaped(text.text.content) + "</text>\n";
            }
        }
        std::string out = "<svg xmlns=\"http://www.w3.org/2000/svg\"" + attribute("width", format(frame.width())) +
                          attribute("height", format(frame.height())) + attribute("viewBox", "0 0 " + format(frame.width()) + " " + format(frame.height())) + ">\n";
        if (!clips.empty()) {
            out += "  <defs>\n";
            for (std::size_t i = 0; i < clips.size(); ++i)
                out += "    <clipPath id=\"clip" + std::to_string(i + 1) + "\"><path" + attribute("d", path_d(clips[i])) + "/></clipPath>\n";
            out += "  </defs>\n";
        }
        return out + body + "</svg>\n";
    }

    void drawing::save_svg(const std::filesystem::path &file) const {
        std::ofstream out{file, std::ios::binary};
        if (!out) throw std::runtime_error("sc::drawing: can't write " + file.string());
        out << to_svg();
        if (!out) throw std::runtime_error("sc::drawing: writing " + file.string() + " failed");
    }
}
