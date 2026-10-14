#include "svg_element.h"
#include "svg_recorder.h"
#include <iostream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <ranges>
#include <sstream>

namespace sc {
    std::ostream &operator<<(std::ostream &lhs, const svg_point &rhs) {
        lhs << "(" << rhs.x << "," << rhs.y << ")";
        return lhs;
    }

    std::ostream &operator<<(std::ostream &lhs, const svg_element &rhs) {
        lhs << rhs.string() << std::endl;
        for (auto const &child: rhs.children) lhs << *child;
        return lhs;
    }

    std::vector<double> string_split_to_double(const std::string &str, const char delim = ' ') {
        std::vector<double> tokens;
        std::stringstream ss(str);
        std::string item;

        while (std::getline(ss, item, delim)) {
            tokens.push_back(std::stod(item));
        }
        return tokens;
    }

    svg_point &svg_point::operator+=(const svg_point &add) {
        x += add.x;
        y += add.y;
        return *this;
    }

    svg_point &svg_point::operator-=(const svg_point &sub) {
        x -= sub.x;
        y -= sub.y;
        return *this;
    }

    svg_point &svg_point::operator*=(const double factor) {
        x *= factor;
        y *= factor;
        return *this;
    }

    svg_point svg_point::operator+(const svg_point &add) const {
        return svg_point{x + add.x, y + add.y};
    }

    svg_point svg_point::operator-(const svg_point &sub) const {
        return svg_point{x - sub.x, y - sub.y};
    }

    void svg_point::rotate(double angle, svg_point center) {
        double angle_radians = angle * M_PI / 180.0;
        double cos_a = cos(angle_radians);
        double sin_a = sin(angle_radians);
        auto norm = *this - center;
        x = cos_a * (norm.x) - sin_a * (norm.y) + center.x;
        y = sin_a * (norm.x) + cos_a * (norm.y) + center.y;
    }

    svg_point svg_point::operator*(double i) const {
        return svg_point{x * i, y * i};
    }

    bool svg_point::operator==(const svg_point &svg_point) const {
        return x == svg_point.x && y == svg_point.y;
    }

    svg_point operator*(double lhs, const svg_point &rhs) {
        return svg_point{lhs * rhs.x, lhs * rhs.y};
    }

    svg_element::svg_element(bool relative) : relative(relative) {
        points.resize(2);
    }

    svg_element::svg_element(const svg_element &other) {
        relative = other.relative;
        fill = other.fill;
        fill_rule = other.fill_rule;
        stroke_color = other.stroke_color;
        stroke_width = other.stroke_width;
        svg = other.svg;
        root = other.root;
        transform = other.transform;
        for (auto p: other.points) points.push_back(p);
        for (auto c: other.children) {
            // add_child() starts a new child where the last one ended, as parsing needs; a copy
            // keeps its own points (a circle's centre, say).
            auto *copy = c->clone();
            const auto own_points = copy->points;
            add_child(copy, false);
            copy->points = own_points;
        }
    }

    svg_element::svg_element(const svg_element &other, svg_element *parent) : svg_element(other) {
        parent->add_child(this, false);
        svg = parent->get_svg();
    }

    svg_element::~svg_element() {
        for (const auto &child: children) {
            delete child;
        }
        delete clip_path; // its own clone (fetch_clip_path); copies don't share it
    }

    color svg_element::get_fill() const {
        return fill;
    }

    color svg_element::get_stroke() const {
        return stroke_color;
    }

    double svg_element::get_stroke_width() const {
        return stroke_width;
    }

    svg_element *svg_element::add_child(svg_element *child, bool is_svg) {
        if (!child) return nullptr;
        if (type() == "Group") {
            if (fill != color::Black && child->get_fill() == color::Black) child->set_fill(fill);
            if (stroke_color != color::Transparent && child->get_stroke() == color::Transparent) child->set_stroke(stroke_color);
            if (stroke_width != 1 && child->get_stroke_width() == 1) child->set_stroke_width(stroke_width);
        }
        auto last_child = children.empty() ? nullptr : children.back();
        children.push_back(child);
        child->parent = this;
        child->root = root;
        if (is_svg) svg = child;
        if (svg) child->svg = svg;
        child->set_start(last_child ? last_child->get_end() : get_start());
        if (last_child) child->previous = last_child;
        return child;
    }

    svg_element *svg_element::add_child(svg_element *child, const std::map<std::string, std::string> &attributes, bool is_svg = false) {
        add_child(child, is_svg);
        child->add_attributes(attributes);
        return child;
    }

    svg_element *svg_element::add_child(svg_element *child, const std::vector<double> &values) {
        add_child(child);
        child->add_attributes(values);
        return child;
    }

    std::string svg_element::path() {
        std::stringstream ss;
        if (parent) ss << parent->path() << " ";
        ss << type();
        return ss.str();
    }

    std::string svg_element::string() const {
        std::stringstream ss;
        if (parent) ss << parent->path() << " ";
        ss << type();
        for (auto const &point: points) ss << " " << point;
        return ss.str();
    }

    svg_point svg_element::get_point(int i) const { return i < 0 || i >= points.size() ? svg_point{0, 0} : points[i]; }
    void svg_element::set_point(int i, double x, double y, bool rel) { set_point(i, {x, y}, rel); }

    void svg_element::set_point(int i, const svg_point pt, const bool rel) {
        if (i > points.size() - 1) points.resize(i + 1);
        points[i] = rel ? pt + get_start() : pt;
    }

    svg_element *svg_element::generator(svg_element *parent, const std::string &name, const std::map<std::string, std::string> &attributes) {
        if (name == "svg") return parent->add_child(new svg_header(), attributes, true);
        if (name == "path") return parent->add_child(new svg_path(), attributes);
        if (name == "clipPath") return parent->add_child(new svg_clip_path(), attributes);
        if (name == "rect") return parent->add_child(new svg_rect(), attributes);
        if (name == "circle") return parent->add_child(new svg_circle(), attributes);
        if (name == "polygon") return parent->add_child(new svg_polygon(), attributes);
        if (name == "use") return parent->add_child(new svg_use(), attributes);
        if (name == "defs") return parent->add_child(new svg_defs(), attributes);
        if (name == "g") {
            auto group = parent->add_child(new svg_group(), attributes);
            for (auto &child: group->children) {
                child->add_attributes(attributes);
            }
            return group;
        }
        std::cerr << "Unhandled element type: " << name << std::endl;
        return nullptr;
    }

    void svg_element::set_fill(const std::string &color_spec) {
        fill = color::from_string(color_spec);
    }

    void svg_element::set_fill(const color &color_spec) {
        fill = color_spec;
    }

    svg_point svg_element::get_start() const { return get_point(0); }
    svg_point svg_element::get_end() const { return get_point(1); }
    void svg_element::set_start(const double x, const double y) { set_start(svg_point{x, y}); }
    void svg_element::set_start_x(const double x) { set_point(0, svg_point{x, get_point(0).y}); }
    void svg_element::set_start_y(const double y) { set_point(0, svg_point{get_point(0).x, y}); }
    void svg_element::set_start(const svg_point &pt) { set_point(0, pt); }
    void svg_element::set_end(const double x, const double y) { set_end(svg_point{x, y}); }
    void svg_element::set_end(const svg_point &pt) { set_point(1, pt); }

    void svg_element::set_relative_end(double x, double y) {
        svg_point endpoint{x, y};
        set_relative_end(endpoint);
    }

    void svg_element::set_relative_end(svg_point &endpoint) {
        if (relative) endpoint += get_start();
        set_end(endpoint);
    }

    void svg_element::add_attributes(const std::map<std::string, std::string> &attributes) {
        for (const auto &[attr, value]: attributes) add_attribute(attr, value);
    }

    void svg_element::add_attributes(const std::vector<double> &attributes) {
    }

    void svg_element::add_attribute(const std::string &name, const std::string &value) {
    }

    std::string svg_element::type() const {
        return "Unknown";
    }

    std::vector<double> svg_element::parse_transform_params(const std::string &s) {
        std::vector<double> params;
        std::stringstream ss;
        for (char ch: s) {
            if (ch == ',' || std::isspace(static_cast<unsigned char>(ch))) ch = ' ';
            ss << ch;
        }
        double val;
        while (ss >> val) {
            params.push_back(val);
        }
        return params;
    }

    void svg_element::parse_transform(const std::string &transform) {
        std::stringstream ss(transform);
        std::string token;
        std::vector<std::pair<std::string, std::vector<double> > > transforms;
        while (std::getline(ss, token, ')')) {
            if (token.empty()) continue;
            auto pos = token.find('(');
            if (pos == std::string::npos) continue;
            transforms.insert(transforms.begin(), {token.substr(0, pos), parse_transform_params(token.substr(pos + 1))});
        }

        for (const auto &[name, params]: transforms) {
            if (name.find("translate") != std::string::npos) {
                double tx = params.size() > 0 ? params[0] : 0;
                double ty = params.size() > 1 ? params[1] : 0;
                normalize({tx, ty});
            } else if (name.find("rotate") != std::string::npos) {
                double angle = params.size() > 0 ? params[0] : 0;
                if (params.size() > 2) rotate(angle, {params[1], params[2]});
                else rotate(angle);
            } else if (name.find("scale") != std::string::npos) {
                double sx = params.size() > 0 ? params[0] : 1;
                double sy = params.size() > 1 ? params[1] : sx;
                scale(sx, sy);
                // } else if (name.find("skewX") != std::string::npos) {
                //     skewX(params.size() > 0 ? params[0] : 0);
                // } else if (name.find("skewY") != std::string::npos) {
                //     skewY(params.size() > 0 ? params[0] : 0);
                // } else if (name.find("matrix") != std::string::npos && params.size() == 6) {
                //     matrix(params[0], params[1], params[2], params[3], params[4], params[5]);
            }
        }
    }

    void svg_element::set_fill_type(svg_recorder *ctx) const {
        ctx->rg(fill);
    }

    void svg_element::do_fill(svg_recorder *ctx) const {
        if (fill_rule == 1) ctx->fStar();
        else ctx->f();
    }

    void svg_element::set_stroke_type(svg_recorder *ctx) const {
        ctx->RG(stroke_color);
        if (stroke_join != 0) ctx->j(stroke_join);
        if (stroke_cap != 0) ctx->J(stroke_cap);
        if (stroke_miter_limit != 10) ctx->M(stroke_miter_limit);
        ctx->w(stroke_width);
    }

    void svg_xml::add_attribute(const std::string &name, const std::string &value) {
        add_common_attribute(name, value);
    }

    std::string svg_xml::type() const {
        return "XML";
    }

    svg_xml::svg_xml() {
        root = this;
    }

    svg_element *svg_xml::clone() const {
        return new svg_xml(*this);
    }

    void svg_header::set_width(const double w) {
        viewBox = rect(viewBox.left(), viewBox.top(), abs(w), viewBox.height());
    }

    void svg_header::set_height(const double h) {
        viewBox = rect(viewBox.left(), viewBox.top(), viewBox.width(), abs(h));
    }

    void svg_header::set_viewBox(const std::vector<double> &values) {
        set_viewBox(values[0], values[1], values[2], values[3]);
    }

    void svg_header::set_viewBox(const double x, const double y, const double w, const double h) {
        viewBox = rect(x, y, w, h);
    }

    void svg_header::add_named_element(svg_element *element) {
        named_elements["#" + element->get_id()] = element;
        named_elements["url(#" + element->get_id() + ")"] = element;
    }

    svg_element *svg_header::get_named_element(const std::string &name) {
        if (!named_elements.contains(name)) return nullptr;
        return named_elements[name];
    }

    svg_element *svg_header::clone() const {
        return new svg_header(*this);
    }

    void svg_header::add_attribute(const std::string &name, const std::string &value) {
        if (name.substr(0, 5) == "xmlns") return;
        if (name == "width") set_width(string_split_to_double(value)[0]);
        else if (name == "height") set_height(string_split_to_double(value)[0]);
        else if (name == "viewBox") set_viewBox(string_split_to_double(value));
        else std::cerr << "Unknown header attribute: " << name << " = " << value << std::endl;
    }


    std::string svg_header::type() const {
        return "SVG";
    }

    std::string svg_header::string() const {
        std::stringstream ss;
        ss << svg_element::string() << " viewBox=" << viewBox;
        return ss.str();
    }

    void svg_group::add_attribute(const std::string &name, const std::string &value) {
        if (add_common_attribute(name, value)) return;
        else std::cerr << "Unknown group attribute: " << name << " = " << value << std::endl;
    }

    std::string svg_group::type() const {
        return "Group";
    }

    std::string svg_group::string() const {
        std::stringstream ss;
        ss << svg_element::string() << " ID=" << id;
        return ss.str();
    }

    void svg_group::flip() {
        svg_element::flip();
    }

    svg_element *svg_group::clone() const {
        return new svg_group(*this);
    }

    void svg_path::add_command(char type, const std::vector<double> &values) {
        if (type == 'm' || type == 'M') add_child(new svg_move(islower(type)), values);
        else if (type == 'c' || type == 'C') add_child(new svg_cubic_bezier(islower(type)), values);
        else if (type == 'q' || type == 'Q') add_child(new svg_quadratic_curve(islower(type)), values);
        else if (type == 's' || type == 'S') add_child(new svg_smooth_cubic_bezier(islower(type)), values);
        else if (type == 'l' || type == 'L') add_child(new svg_line(islower(type)), values);
        else if (type == 'h' || type == 'H') add_child(new svg_horizontal_line(islower(type)), values);
        else if (type == 'v' || type == 'V') add_child(new svg_vertical_line(islower(type)), values);
        else if (type == 'a' || type == 'A') add_child(new svg_arc(islower(type)), values);
        else if (type == 'z' || type == 'Z') add_child(new svg_close(), values);
        else std::cerr << "Unknown command: " << type << " " << values.size() << std::endl;
    }

    bool svg_path::isdecimal(const char character) {
        return character == '.';
    }

    bool svg_path::issign(const char character) {
        return character == '+' || character == '-';
    }

    double svg_path::get_double(std::istringstream &ss) {
        int next = ss.peek(); // int: EOF is not a char
        while (isspace(next) || next == ',') {
            ss.get();
            next = ss.peek();
        }
        std::string value;
        bool gotdecimal = false;
        bool gotsign = false;
        next = ss.peek();
        while (next != EOF && (std::isdigit(next) || issign(next) || isdecimal(next))) {
            if (issign(next)) {
                if (gotsign) break;
            }
            if (isdecimal(next)) {
                if (gotdecimal) break;
                gotdecimal = true;
            }
            gotsign = true;
            value += static_cast<char>(ss.get());
            next = ss.peek();
        }
        if (value.empty()) return 0;
        return std::stod(value);
    }

    void svg_path::parse_path(const std::string &value) {
        std::istringstream ss(value);
        ss.imbue(std::locale::classic());
        char c;
        while (ss >> c) {
            std::vector<double> args;
            while (ss.peek() != EOF && !(isalpha(ss.peek()))) args.push_back(get_double(ss));
            add_command(c, args);
        }
    }

    void svg_element::set_stroke(const std::string &color_spec) {
        stroke_color = color::from_string(color_spec);
    }

    void svg_element::set_stroke(const color &color_spec) {
        stroke_color = color_spec;
    }

    void svg_element::set_stroke_width(double new_width) {
        stroke_width = new_width;
    }

    void svg_path::add_attribute(const std::string &name, const std::string &value) {
        if (add_common_attribute(name, value)) return;
        else if (name == "fill") set_fill(value);
        else if (name == "stroke") set_stroke(value);
        else if (name == "stroke-width") set_stroke_width(std::stod(value));
        else if (name == "d") parse_path(value);
        else if (name == "id") id = value;
        else std::cerr << "Unknown path attribute: " << name << " = " << value << std::endl;
    }

    std::string svg_path::type() const {
        return "Path";
    }

    std::string svg_path::string() const {
        std::stringstream ss;
        if (fill != color{0, 0, 0, 0}) {
            ss << svg_element::string()
                    << " fill=rgba(" << fill.red() << "," << fill.green() << "," << fill.blue() << "," << fill.alpha()
                    << ")";
        }
        if (stroke_color != color{0, 0, 0, 1}) {
            ss << " stroke=rgba(" << stroke_color.red() << "," << stroke_color.green() << "," << stroke_color.blue() << "," << stroke_color.alpha()
                    << ")";
        }
        if (stroke_width != 1) {
            ss << " stroke-width=" << stroke_width;
        }
        return ss.str();
    }

    svg_element *svg_path::clone() const {
        return new svg_path(*this);
    }

    void svg_path::stroke(svg_recorder *ctx) const {
        svg_element::stroke(ctx);
    }

    void svg_move::add_attributes(const std::vector<double> &values) {
        set_relative_end(values[0], values[1]);
        parent->set_start(get_end());
        if (values.size() >= 4) {
            const std::vector<double> rest{values.begin() + 2, values.end()};
            parent->add_child(new svg_line(relative), rest);
        }
    }

    std::string svg_move::type() const {
        return "Move";
    }

    svg_element *svg_move::clone() const {
        return new svg_move(*this);
    }

    void svg_line::add_attributes(const std::vector<double> &values) {
        set_relative_end(values[0], values[1]);
        if (values.size() >= 4) {
            const std::vector<double> rest{values.begin() + 2, values.end()};
            parent->add_child(new svg_line(relative), rest);
        }
    }

    std::string svg_line::type() const {
        return "Line";
    }

    svg_element *svg_line::clone() const {
        return new svg_line(*this);
    }

    void svg_horizontal_line::add_attributes(const std::vector<double> &values) {
        const svg_point endpoint{relative ? values[0] + get_start().x : values[0], get_start().y};
        set_end(endpoint);
    }

    void svg_vertical_line::add_attributes(const std::vector<double> &values) {
        const svg_point endpoint{get_start().x, relative ? values[0] + get_start().y : values[0]};
        set_end(endpoint);
    }

    void svg_arc::add_attributes(const std::vector<double> &values) {
        radius = svg_point{values[0], values[1]};
        x_axis_rotation = values[2];
        large_arc_flag = values[3] > 0.5;
        sweep_flag = values[4] > 0.5;
        set_relative_end(values[5], values[6]);
        if (values.size() >= 14) {
            std::vector<double> rest{values.begin() + 7, values.end()};
            parent->add_child(new svg_arc(relative), rest);
        }
    }

    std::string svg_arc::type() const {
        return "Arc";
    }

    std::string svg_arc::string() const {
        std::stringstream ss;
        ss << svg_element::string()
                << " radius=" << radius
                << " x_axis_rotation=" << x_axis_rotation
                << " large_arc_flag=" << (large_arc_flag ? "Yes" : "No")
                << " sweep_flag=" << (sweep_flag ? "Yes" : "No");
        return ss.str();
    }

    double svg_arc::angleBetween(double ux, double uy, double vx, double vy) {
        double dot = ux * vx + uy * vy;
        double len = std::sqrt((ux * ux + uy * uy) * (vx * vx + vy * vy));
        double ang = std::acos(std::max(-1.0, std::min(1.0, dot / len)));
        if (ux * vy - uy * vx < 0) ang = -ang;
        return ang;
    };

    void svg_arc::stroke(svg_recorder *ctx) const {
        // ctx->l(get_end().x, get_end().y);

        auto x1 = get_start().x;
        auto y1 = get_start().y;
        auto x2 = get_end().x;
        auto y2 = get_end().y;
        auto rx = radius.x;
        auto ry = radius.y;
        auto angle = x_axis_rotation;


        // Convert angle to radians
        double phi = angle * M_PI / 180.0;
        double cosPhi = std::cos(phi);
        double sinPhi = std::sin(phi);

        // Step 1: Compute transformed coordinates
        double dx = (x1 - x2) / 2.0;
        double dy = (y1 - y2) / 2.0;
        double x1p = cosPhi * dx + sinPhi * dy;
        double y1p = -sinPhi * dx + cosPhi * dy;

        // Correct radii
        double rx2 = rx * rx;
        double ry2 = ry * ry;
        double x1p2 = x1p * x1p;
        double y1p2 = y1p * y1p;
        double lambda = x1p2 / rx2 + y1p2 / ry2;
        if (lambda > 1.0) {
            double scale = std::sqrt(lambda);
            rx *= scale;
            ry *= scale;
            rx2 = rx * rx;
            ry2 = ry * ry;
        }

        // Step 2: Compute center
        double sign = (large_arc_flag == sweep_flag) ? -1.0 : 1.0;
        double num = rx2 * ry2 - rx2 * y1p2 - ry2 * x1p2;
        double den = rx2 * y1p2 + ry2 * x1p2;
        double factor = sign * std::sqrt(std::max(0.0, num / den));
        double cxp = factor * (rx * y1p / ry);
        double cyp = factor * (-ry * x1p / rx);

        double cx = cosPhi * cxp - sinPhi * cyp + (x1 + x2) / 2.0;
        double cy = sinPhi * cxp + cosPhi * cyp + (y1 + y2) / 2.0;

        double theta1 = angleBetween(1, 0, (x1p - cxp) / rx, (y1p - cyp) / ry);
        double deltaTheta = angleBetween((x1p - cxp) / rx, (y1p - cyp) / ry, (-x1p - cxp) / rx, (-y1p - cyp) / ry);

        if (!sweep_flag && deltaTheta > 0) deltaTheta -= 2 * M_PI;
        else if (sweep_flag && deltaTheta < 0) deltaTheta += 2 * M_PI;

        // Step 4: Split arc into segments of max 90°
        int segments = static_cast<int>(std::ceil(std::fabs(deltaTheta / (M_PI / 2.0))));
        double delta = deltaTheta / segments;

        for (int i = 0; i < segments; ++i) {
            double t1 = theta1 + i * delta;
            double t2 = t1 + delta;

            // Endpoints
            double cosT1 = std::cos(t1), sinT1 = std::sin(t1);
            double cosT2 = std::cos(t2), sinT2 = std::sin(t2);

            std::pair<double, double> P0 = {
                cx + rx * cosPhi * cosT1 - ry * sinPhi * sinT1,
                cy + rx * sinPhi * cosT1 + ry * cosPhi * sinT1
            };
            std::pair<double, double> P3 = {
                cx + rx * cosPhi * cosT2 - ry * sinPhi * sinT2,
                cy + rx * sinPhi * cosT2 + ry * cosPhi * sinT2
            };

            // Derivatives
            double alpha = std::tan(delta / 4.0);
            double factor = (8.0 / 5) * alpha / (1 + alpha * alpha);

            std::pair<double, double> P1 = {
                P0.first - factor * (rx * cosPhi * sinT1 + ry * sinPhi * cosT1),
                P0.second - factor * (rx * sinPhi * sinT1 - ry * cosPhi * cosT1)
            };
            std::pair<double, double> P2 = {
                P3.first + factor * (rx * cosPhi * sinT2 + ry * sinPhi * cosT2),
                P3.second + factor * (rx * sinPhi * sinT2 - ry * cosPhi * cosT2)
            };

            ctx->c(P1.first, P1.second, P2.first, P2.second, P3.first, P3.second);
            //   curves.push_back({P0,P1,P2,P3});
        }
    }

    void svg_arc::scale(double scale_x, double scale_y) {
        radius.x *= scale_x;
        radius.y *= scale_y;
        svg_element::scale(scale_x, scale_y);
    }

    svg_element *svg_arc::clone() const {
        return new svg_arc(*this);
    }

    void svg_close::add_attributes(const std::vector<double> &values) {
        set_end(parent->get_start());
    }

    std::string svg_close::type() const {
        return "Close";
    }


    void svg_cubic_bezier::add_attributes(const std::vector<double> &values) {
        set_point(2, values[0], values[1], relative);
        set_point(3, values[2], values[3], relative);
        set_relative_end(values[4], values[5]);
        if (values.size() >= 12) {
            std::vector<double> rest{values.begin() + 6, values.end()};
            parent->add_child(new svg_cubic_bezier(relative), rest);
        }
    }

    std::string svg_cubic_bezier::type() const {
        return "Cubic Bezier";
    }

    svg_element *svg_quadratic_curve::clone() const {
        return new svg_quadratic_curve(*this);
    }

    void svg_smooth_cubic_bezier::add_attributes(const std::vector<double> &values) {
        set_point(2, (get_start() * 2) - previous->get_point(3));
        set_point(3, values[0], values[1], relative);
        set_relative_end(values[2], values[3]);
        if (values.size() >= 8) {
            std::vector<double> rest{values.begin() + 4, values.end()};
            parent->add_child(new svg_smooth_cubic_bezier(relative), rest);
        }
    }


    svg_element *svg_circle::clone() const {
        return new svg_circle(*this);
    }

    void svg_circle::add_attributes(const std::map<std::string, std::string> &attributes) {
        svg_element::add_attributes(attributes);
    }

    void svg_circle::add_attribute(const std::string &name, const std::string &value) {
        if (add_common_attribute(name, value)) return;
        else if (name == "r") radius = std::stod(value);
        else if (name == "cx") points[0].x = std::stod(value);
        else if (name == "cy") points[0].y = std::stod(value);
        else std::cerr << "Unknown circle attribute: " << name << " = " << value << std::endl;
    }

    std::string svg_circle::string() const {
        std::stringstream ss;
        ss << svg_element::string() << " fill=rgba(" << fill.red() << "," << fill.green() << "," << fill.blue() << "," << fill.alpha() << ")";
        ss << " radius=(" << radius << ")";
        return ss.str();
    }

    std::string svg_circle::type() const {
        return "Circle";
    }

    void svg_circle::scale(double scale_x, double scale_y) {
        svg_element::scale(scale_x, scale_y);
        radius *= scale_x; // If not round??
    }


    void svg_circle::stroke(svg_recorder *ctx) const {
        double m = 0.5522847498;
        double center_x = points[0].x; //  + radius;
        double center_y = points[0].y; //  + radius;
        double k = radius * m;

        ctx->m(center_x, center_y + radius);
        ctx->c(center_x + k, center_y + radius, center_x + radius, center_y + k, center_x + radius, center_y);
        ctx->c(center_x + radius, center_y - radius + k, center_x + k, center_y - radius, center_x, center_y - radius);
        ctx->c(center_x - k, center_y - radius, center_x - radius, center_y - k, center_x - radius, center_y);
        ctx->c(center_x - radius, center_y + k, center_x - k, center_y + radius, center_x, center_y + radius);
        ctx->h();
    }

    void svg_circle::draw(svg_recorder *ctx) const {
        if (clip_path) start_clip(ctx);
        if (fill.alpha() != 0) {
            ctx->q();
            set_fill_type(ctx);
            stroke(ctx);
            do_fill(ctx);;
            ctx->Q();
        }

        if (stroke_color.alpha() != 0) {
            ctx->q();
            set_stroke_type(ctx);
            stroke(ctx);
            ctx->s();
            ctx->Q();
        }
        if (clip_path) end_clip(ctx);
    }


    void svg_rect::add_attributes(const std::map<std::string, std::string> &attributes) {
        svg_element::add_attributes(attributes);
        set_point(2, width, height);
    }

    double svg_element::width_value(const std::string &value) const {
        if (value.ends_with("%")) {
            return std::stod(value.substr(0, value.size() - 1)) / 100.0 * svg->width();
        }
        return std::stod(value);
    }

    double svg_element::height_value(const std::string &value) const {
        if (value.ends_with("%")) {
            return std::stod(value.substr(0, value.size() - 1)) / 100.0 * svg->height();
        }
        return std::stod(value);
    }

    void svg_rect::add_attribute(const std::string &name, const std::string &value) {
        if (add_common_attribute(name, value)) return;
        else if (name == "width") width = width_value(value);
        else if (name == "height") height = height_value(value);
        else if (name == "rx") radius_x = std::stod(value);
        else if (name == "ry") radius_y = std::stod(value);
        else std::cerr << "Unknown rect attribute: " << name << " = " << value << std::endl;
    }


    std::string svg_rect::string() const {
        std::stringstream ss;
        ss << svg_element::string() << " fill=rgba(" << fill.red() << "," << fill.green() << "," << fill.blue() << "," << fill.alpha() << ")";
        if (radius_x > 0 || radius_y > 0) ss << " radius=(" << radius_x << "," << radius_y << ")";
        return ss.str();
    }

    std::string svg_rect::type() const {
        return "Rect";
    }

    void svg_rect::scale(double scale_x, double scale_y) {
        width *= scale_x;
        height *= scale_y;
        radius_x *= std::fabs(scale_x);
        radius_y *= std::fabs(scale_y);
        svg_element::scale(scale_x, scale_y);
    }

    void svg_rect::flip() {
        height = -height;
        svg_element::flip();
    }

    void svg_header::scale(double scale_x, double scale_y) {
        set_width(width() * scale_x);
        set_height(height() * scale_y);
        svg_element::scale(scale_x, scale_y);
    }

    void svg_use::add_attribute(const std::string &name, const std::string &value) {
        if (name == "x") offset.x = std::stod(value);
        else if (name == "y") offset.y = std::stod(value);
        else if (add_common_attribute(name, value)) return;
        else if (name == "xlink:href") fetch_named_element(value);
        else std::cerr << "Unknown use attribute: " << name << " = " << value << std::endl;
    }

    std::string svg_use::type() const {
        return "Use";
    }

    std::string svg_use::string() const {
        std::stringstream ss;
        ss << svg_element::string();
        return ss.str();
    }

    svg_element *svg_use::clone() const {
        return new svg_use(*this);
    }

    void svg_use::draw(svg_recorder *ctx) const {
        if (clip_path) start_clip(ctx);
        for (auto const &child: children) child->draw(ctx);
        if (clip_path) end_clip(ctx);
    }

    void svg_element::draw(svg_recorder *ctx) const {
        for (auto const &child: children) child->draw(ctx);
    }

    void svg_element::scale(double scale_x, double scale_y) {
        for (auto &p: points) {
            p.x *= scale_x;
            p.y *= scale_y;
        }
        if (stroke_width != 1) stroke_width *= scale_x;
        for (auto &child: children) child->scale(scale_x, scale_y);
    }

    double svg_element::width() const {
        if (type() == "XML" && !children.empty()) return children[0]->width();
        if (parent && parent->type() != "XML") return parent->width();
        return 100;
    }

    double svg_element::height() const {
        if (type() == "XML" && !children.empty()) return children[0]->height();
        if (parent && parent->type() != "XML") return parent->height();
        return 100;
    }

    rect svg_element::get_viewBox() const {
        if (type() == "XML" && !children.empty()) return children[0]->get_viewBox();
        if (parent && parent->type() != "XML") return parent->get_viewBox();
        return {0, 0};
    }

    void svg_element::set_id(const std::string &value) {
        id = value;
        reinterpret_cast<svg_header *>(svg)->add_named_element(this);
    }

    void svg_element::trim(std::string &name) {
        size_t start = name.find_first_not_of(" \t\n\r");
        size_t end = name.find_last_not_of(" \t\n\r");
        if (start == std::string::npos) {
            name.clear();
            return;
        }
        name = name.substr(start, end - start + 1);
    }

    double svg_header::width() const {
        return viewBox.width();
    }

    double svg_header::height() const {
        return viewBox.height();
    }

    rect svg_header::get_viewBox() const {
        return viewBox;
    }


    void svg_element::add_styles(const std::string &style) {
        std::stringstream ss(style);
        std::string item;

        while (std::getline(ss, item, ';')) {
            if (item.empty()) continue;
            auto pos = item.find(':');
            if (pos == std::string::npos) continue;

            std::string name = item.substr(0, pos);
            std::string value = item.substr(pos + 1);

            trim(name);
            trim(value);

            add_style(name, value);
        }
    }

    void svg_element::add_style(const std::string &name, const std::string &value) {
        if (name == "fill") set_fill(value);
        else if (name == "stroke") set_stroke(value);
        else if (name == "stroke-width") set_stroke_width(std::stod(value));
        else std::cerr << "Unknown style attribute: " << name << " = " << value << std::endl;
    }


    bool svg_element::add_common_attribute(const std::string &name, const std::string &value) {
        if (name == "id") set_id(value);
        else if (name == "fill") set_fill(value);
        else if (name == "stroke") set_stroke(value);
        else if (name == "stroke-width") set_stroke_width(std::stod(value));
        else if (name == "stroke-linecap") stroke_cap = (value == "round" ? 1 : (value == "square" ? 2 : 0));
        else if (name == "stroke-linejoin") stroke_join = (value == "round" ? 1 : (value == "bevel" ? 2 : 0));
        else if (name == "stroke-miterlimit") stroke_miter_limit = std::stod(value);
        else if (name == "fill-rule") fill_rule = (value == "evenodd" ? 1 : 0);
        else if (name == "paint-order") return true;
        else if (name == "transform") transform = value;
        else if (name == "clip-path") fetch_clip_path(value);
        else if (name == "x") set_start_x(width_value(value));
        else if (name == "y") set_start_y(height_value(value));
        else if (name == "style") add_styles(value);
        else return false;
        return true;
    }

    void svg_element::resize(int new_width, int new_height, bool keep_aspect) {
        double scale_x = new_width / width();
        double scale_y = new_height / height();
        if (keep_aspect) {
            scale_x = scale_y = std::min(scale_x, scale_y);
        }
        scale(scale_x, scale_y);
    }

    void svg_element::fetch_clip_path(const std::string &name) {
        auto element = reinterpret_cast<svg_header *>(svg)->get_named_element(name);
        if (!element) return; // Named element not found :(
        delete clip_path;
        clip_path = element->clone();
    }

    void svg_element::start_clip(svg_recorder *ctx) const {
        ctx->q();
        clip_path->stroke(ctx);
        ctx->W();
        ctx->n();
    }

    void svg_element::end_clip(svg_recorder *ctx) {
        ctx->Q();
    }

    void svg_element::fetch_named_element(const std::string &name) {
        auto child = reinterpret_cast<svg_header *>(svg)->get_named_element(name);
        if (!child) return; // Named element not found :(
        child = child->clone();

        // Inherit styles
        if (fill != color::Black) child->set_fill(fill);
        if (stroke_color != color::Transparent) child->set_stroke(stroke_color);
        if (stroke_width != 1) child->set_stroke_width(stroke_width);

        add_child(child, false);
    }

    void svg_element::rotate(double angle, svg_point center) {
        for (auto &p: points) p.rotate(angle, center);
        for (auto &child: children) child->rotate(angle, center);
    }

    void svg_element::normalize() {
        auto vb = get_viewBox();
        svg_point mv{-vb.left(), -vb.top()};
        normalize(mv);
    }

    void svg_element::normalize(svg_point mv) {
        if (clip_path) clip_path->normalize(mv);
        for (auto &child: children) child->normalize(mv);
        for (auto &p: points) p += mv;
    }


    void svg_use::normalize(svg_point mv) {
        svg_element::normalize(mv);
    }

    std::string svg_defs::type() const {
        return svg_use::type();
    }

    void svg_defs::draw(svg_recorder *ctx) const {
        return; // Definitions don't get drawn...
    }

    void svg_clip_path::draw(svg_recorder *ctx) const {
        return; // Clip paths don't get drawn...
    }

    void svg_header::normalize(svg_point mv) {
        svg_element::normalize(mv);
        viewBox = {0, 0, width(), height()};
    }

    std::string svg_element::get_id() {
        return id;
    }

    void svg_element::do_transform() {
        for (auto &child: children) child->do_transform();
        if (!transform.empty()) {
            parse_transform(transform);
            transform = "";
        }
    }

    void svg_use::do_transform() {
        for (auto &child: children) child->do_transform();
        svg_element::normalize(offset);
        svg_element::do_transform();
        offset = svg_point({0, 0});

    }

    void svg_element::flip() {
        if (svg) for (auto &p: points) p.y = svg->height() - p.y;
        for (auto &child: children) { child->flip(); }
    }

    void svg_arc::flip() {
        svg_element::flip();
        sweep_flag = !sweep_flag;
    }

    svg_element *svg_element::get_svg() const {
        return svg;
    }

    std::vector<svg_element *> svg_element::get_children() {
        return children;
    }

    void svg_element::stroke(svg_recorder *ctx) const {
        for (auto const &child: children) { child->stroke(ctx); }
    }

    void svg_path::draw(svg_recorder *ctx) const {
        if (clip_path) start_clip(ctx);
        if (fill.alpha() != 0) {
            set_fill_type(ctx);
            stroke(ctx);
            do_fill(ctx);
        }
        if (stroke_color.alpha() != 0) {
            set_stroke_type(ctx);
            stroke(ctx);
            ctx->S();
        }
        if (clip_path) end_clip(ctx);
    }

    std::string svg_clip_path::type() const {
        return "ClipPath";
    }

    void svg_move::stroke(svg_recorder *ctx) const {
        ctx->m(get_end().x, get_end().y);
    }

    void svg_line::stroke(svg_recorder *ctx) const {
        ctx->l(get_end().x, get_end().y);
    }

    void svg_close::stroke(svg_recorder *ctx) const {
        ctx->h();
    }

    svg_element *svg_close::clone() const {
        return new svg_close(*this);
    }

    void svg_cubic_bezier::stroke(svg_recorder *ctx) const {
        ctx->c(points[2].x, points[2].y, points[3].x, points[3].y, points[1].x, points[1].y);
    }

    svg_element *svg_cubic_bezier::clone() const {
        return new svg_cubic_bezier(*this);
    }

    void svg_quadratic_curve::add_attributes(const std::vector<double> &values) {
        set_point(2, values[0], values[1], relative);
        set_relative_end(values[2], values[3]);
        if (values.size() >= 8) {
            std::vector<double> rest{values.begin() + 4, values.end()};
            parent->add_child(new svg_quadratic_curve(relative), rest);
        }
    }

    std::string svg_quadratic_curve::type() const {
        return "QuadraticCurve";
    }

    void svg_quadratic_curve::stroke(svg_recorder *ctx) const {
        auto c1 = get_start() + ((2 / 3.0) * (get_point(2) - get_start()));
        auto c2 = get_end() + ((2 / 3.0) * (get_point(2) - get_end()));
        ctx->c(c1.x, c1.y, c2.x, c2.y, points[1].x, points[1].y);
    }

    void svg_rect::stroke(svg_recorder *ctx) const {
        const double x0 = get_start().x, y0 = get_start().y, x1 = x0 + width, y1 = y0 + height;
        // SVG: rx or ry alone sets both, and neither is more than half the side.
        double rx = radius_x > 0 ? radius_x : radius_y;
        double ry = radius_y > 0 ? radius_y : radius_x;
        rx = std::min(rx, std::fabs(width) / 2);
        ry = std::min(ry, std::fabs(height) / 2);
        if (rx <= 0 || ry <= 0) {
            ctx->re(x0, y0, width, height);
            return;
        }
        // width and height turn negative when flipped, so the corners go the way they point.
        rx = std::copysign(rx, width);
        ry = std::copysign(ry, height);
        constexpr double k = 1 - 0.5522847498; // a quarter circle as a Bezier curve: control points from the corner
        ctx->m(x0 + rx, y0);
        ctx->l(x1 - rx, y0);
        ctx->c(x1 - rx * k, y0, x1, y0 + ry * k, x1, y0 + ry);
        ctx->l(x1, y1 - ry);
        ctx->c(x1, y1 - ry * k, x1 - rx * k, y1, x1 - rx, y1);
        ctx->l(x0 + rx, y1);
        ctx->c(x0 + rx * k, y1, x0, y1 - ry * k, x0, y1 - ry);
        ctx->l(x0, y0 + ry);
        ctx->c(x0, y0 + ry * k, x0 + rx * k, y0, x0 + rx, y0);
        ctx->h();
    }

    void svg_rect::draw(svg_recorder *ctx) const {
        if (clip_path) start_clip(ctx);
        if (fill.alpha() != 0) {
            ctx->q();
            set_fill_type(ctx);
            stroke(ctx);
            do_fill(ctx);;
            ctx->Q();
        }
        if (stroke_color.alpha() != 0) {
            ctx->q();
            set_stroke_type(ctx);
            stroke(ctx);
            ctx->s();
            ctx->Q();
        }
        if (clip_path) end_clip(ctx);
    }

    svg_element *svg_rect::clone() const {
        return new svg_rect(*this);
    }

    void svg_polygon::add_attribute(const std::string &name, const std::string &value) {
        if (name == "points") {
            parse_path("M" + value);
            parse_path("Z");
        } else {
            svg_path::add_attribute(name, value);
        }
    }

    std::string svg_polygon::type() const {
        return "Polygon";
    }

    void svg_polygon::set_start(const svg_point &pt) {
        svg_element::set_start(pt);
    }
} // sc
