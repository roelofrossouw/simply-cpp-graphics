// sc::graphics::canvas is the drawing interface sc-image, sc-pdflite and sc-mupdf implement. This demo is a canvas
// that only prints what it's asked to paint: every shape, text and SVG drawing comes down to a few
// primitives.

#include <iostream>
#include <string>

#include <sc/core/sc.h>

#include <sc/graphics/canvas.h>
#include <sc/graphics/paragraph.h>

namespace {
    // [readme]
    // A canvas implements the primitives; canvas::draw() does the rest.
    class printing_canvas : public sc::graphics::canvas {
    protected:
        void paint_polygon(const sc::polygon &shape, const sc::graphics::appearance &look) override {
            print("polygon of " + std::to_string(shape.size()) + " points", look);
        }

        void paint_circle(const sc::circle &shape, const sc::graphics::appearance &look) override {
            print("circle, radius " + sc::console::format(shape.radius()), look);
        }

        void paint_line(const sc::line &shape, const sc::graphics::stroke &stroke) override {
            print("line " + sc::console::format(shape.from()) + " to " + sc::console::format(shape.to()), stroke);
        }

        void paint_text(const sc::graphics::text &sample, const sc::graphics::fill &fill) override {
            sc::console::note("text \"" + sample.content + "\" in " + sample.font.family + " " + sc::console::format(sample.font.size) +
                              ", " + sc::console::format(fill.colour()));
        }

        void paint_bitmap(const sc::graphics::bitmap &picture, const sc::transform &placement) override {
            sc::console::note("bitmap " + sc::console::format(picture.size()) + " to " + sc::console::format(placement.apply(sc::rect{0, 0, 1, 1}).bounds()));
        }

        double measure_width(const sc::graphics::text &sample) override { return sample.content.size() * sample.font.size / 2; }
        double measure_x_height(const sc::graphics::font &font) override { return font.size / 2; }
        double measure_cap_height(const sc::graphics::font &font) override { return font.size * 0.7; }

    private:
        static void print(const std::string &what, const sc::graphics::appearance &look) {
            const auto &stroke = look.stroke();
            const auto &fill = look.fill();
            std::string how;
            if (stroke.visible()) how += " outlined " + sc::console::format(stroke.colour()) + ", " + sc::console::format(stroke.width()) + " wide";
            if (fill.visible()) how += " filled " + sc::console::format(fill.colour());
            sc::console::note(what + how);
        }
    };
    // [/readme]
}

int main() {
    try {
        sc::console::title("simply-cpp graphics: what a canvas is asked to paint");
        printing_canvas canvas;

        sc::console::heading("Shapes: draw(shape, appearance = {stroke: black 1 wide, fill: none})");
        SC_STEP(canvas.draw(sc::rect{10, 10, 80, 40}));
        SC_STEP(canvas.draw(sc::rounded_rect{sc::rect{10, 10, 80, 40}, 6}, {sc::graphics::stroke{sc::color::Red, 2}, sc::color{"gold"}})); // a polygon of arcs here
        SC_STEP(canvas.draw(sc::circle{{50, 50}, 20}, sc::graphics::appearance::filled(sc::color::Blue)));
        SC_STEP(canvas.draw(sc::rotated_rect{{50, 50}, {40, 20}, 30}));
        SC_STEP(canvas.draw(sc::line{{0, 0}, {100, 0}}, sc::graphics::stroke{sc::color::Green, 0.5}));

        sc::console::heading("Text: sc::graphics::text{content, where its baseline starts, sc::graphics::font}");
        const sc::graphics::text label{"simply-cpp", {10, 30}, sc::graphics::font::bold("serif", 18)};
        SC_STEP(canvas.draw(label));
        SC_SHOW(canvas.text_width(label));

        sc::console::heading("Installed fonts: a font's face, and the face's typeface (fontconfig, FreeType)");
        const sc::graphics::font body{"serif", 12};
        SC_SHOW(body.available());
        SC_SHOW(sc::graphics::font{"No Such Font"}.available()); // fontconfig would substitute one
        SC_SHOW(body.width("simply-cpp")); // points at 12 pt: what sc-pdflite and sc-mupdf draw
        SC_SHOW(body.cap_height());
        SC_SHOW(body.line_height());
        if (const auto face = body.face()) {
            SC_SHOW(face->typeface().name());
            SC_SHOW(face->style());
            SC_SHOW(face->weight());
            SC_SHOW(face->typeface().faces().size()); // its styles: regular, bold, italic, ...
            if (const auto bold = face->typeface().face(700)) SC_SHOW(bold->style());
        }
        SC_SHOW(sc::graphics::typeface::installed().size());
        SC_SHOW(sc::graphics::wrap("Lines break at spaces, as wide as the font allows.", body, 120));

        sc::console::heading("Paragraphs: wrapped and aligned in a rect, measured by the canvas");
        const sc::graphics::paragraph text{"A paragraph wraps at its area's width; what doesn't fit its height comes back.",
                                 {10, 10, 120, 40}, {"sans-serif", 10}, sc::graphics::align::left};
        std::string rest;
        SC_STEP(rest = canvas.draw(text));
        SC_SHOW(rest);

        sc::console::heading("A drawing: shapes and text as data, read from SVG or built, drawn anywhere");
        auto logo = sc::graphics::drawing::from_svg(R"(<svg xmlns="http://www.w3.org/2000/svg" width="320" height="200">)"
                                          R"(<rect width="320" height="200" fill="#2a6f97"/>)"
                                          R"(<circle cx="100" cy="100" r="70" fill="#f4d35e"/></svg>)");
        SC_SHOW(logo.size());
        SC_STEP(logo.add(sc::graphics::text{"logo", {200, 110}, {"sans-serif", 24}}, sc::color::White));
        SC_STEP(logo.flip_horizontal()); // across its page: the circle to the right, the text to the left
        SC_STEP(canvas.draw(logo, sc::rect{0, 0, 160, 160})); // scaled to fit, centred
        SC_SHOW(logo.to_svg().size());
    } catch (const std::exception &error) {
        std::cerr << "sc-graphics-demo: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
