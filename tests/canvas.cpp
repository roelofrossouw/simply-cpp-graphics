// sc::canvas turns every draw() into a few primitives; a canvas that records them shows which, with
// what, and what it leaves out.
#include <canvas.h>

#include "sc_test.h"

#include <cmath>
#include <string>
#include <vector>

namespace {
    struct recording_canvas : sc::canvas {
        std::vector<std::string> calls;
        sc::polygon last_polygon;
        sc::stroke last_stroke{sc::color::Green, -1};
        sc::fill last_fill;

    protected:
        void paint_polygon(const sc::polygon &shape, const sc::appearance &look) override {
            calls.push_back("polygon " + std::to_string(shape.size()));
            last_polygon = shape;
            last_stroke = look.stroke();
            last_fill = look.fill();
        }

        void paint_circle(const sc::circle &shape, const sc::appearance &look) override {
            calls.push_back("circle " + std::to_string(static_cast<int>(shape.radius())));
            last_stroke = look.stroke();
            last_fill = look.fill();
        }

        void paint_line(const sc::line &, const sc::stroke &stroke) override {
            calls.push_back("line " + std::to_string(static_cast<int>(stroke.width())));
        }

        void paint_text(const sc::text &sample, const sc::fill &) override {
            calls.push_back("text " + sample.content + " " + sample.font.family);
        }

        // Half an em per character, x-height half the size, capitals 0.7 of it.
        double measure_width(const sc::text &sample) override { return sample.content.size() * sample.font.size / 2; }

        double measure_x_height(const sc::font &font) override { return font.size / 2; }

        double measure_cap_height(const sc::font &font) override { return font.size * 0.7; }

        void push_clip(const sc::path &shape) override { calls.push_back("clip " + std::to_string(shape.steps().size())); }

        void pop_clip() override { calls.push_back("unclip"); }
    };

    // A shape of its own: bounds() and a to_path() next to it make it an sc::geometry.
    struct diamond {
        sc::point centre;
        double radius;

        [[nodiscard]] sc::rect bounds() const { return {centre.x() - radius, centre.y() - radius, 2 * radius, 2 * radius}; }
    };

    sc::path to_path(const diamond &shape) {
        sc::path result;
        const double x = shape.centre.x(), y = shape.centre.y(), r = shape.radius;
        result.move_to({x, y - r}).line_to({x + r, y}).line_to({x, y + r}).line_to({x - r, y}).close();
        return result;
    }

    static_assert(sc::geometry<diamond>);
}

int main() {
    SECTION("Shapes become primitives");
    {
        recording_canvas canvas;
        canvas.draw(sc::rect{10, 20, 30, 40});
        CHECK_EQ(canvas.calls.back(), std::string{"polygon 4"});
        CHECK_EQ(canvas.last_polygon[2], (sc::point{40, 60}));
        CHECK(canvas.last_stroke == sc::stroke{}); // the defaults: black outline 1 wide, no fill
        CHECK(canvas.last_stroke.colour() == sc::color::Black);
        CHECK_EQ(canvas.last_stroke.width(), 1.0);
        CHECK(!canvas.last_fill.visible());

        canvas.draw(sc::rect_i{1, 2, 3, 4}, {sc::stroke{sc::color::Red, 2}, sc::color::Blue});
        CHECK_EQ(canvas.last_polygon[0], (sc::point{1, 2}));
        CHECK(canvas.last_fill.colour() == sc::color::Blue);
        CHECK_EQ(canvas.last_stroke.width(), 2.0);
        canvas.draw(sc::rect{1, 2, 3, 4}, sc::color::Red); // a colour is a stroke 1 wide
        CHECK(canvas.last_stroke == (sc::stroke{sc::color::Red, 1}));

        canvas.draw(sc::rotated_rect{{0, 0}, {2, 2}, 45});
        CHECK_NEAR(canvas.last_polygon[0].y(), -std::sqrt(2.0), 1e-12); // exact corners

        canvas.draw(sc::rotated_rect_i{{10, 10}, {4, 2}, 90}); // every flavour of each shape
        CHECK_EQ(canvas.calls.back(), std::string{"polygon 4"});
        CHECK_NEAR(canvas.last_polygon[0].x(), 11.0, 1e-12);

        canvas.draw(sc::polygon_i{{0, 0}, {5, 0}, {5, 5}});
        CHECK_EQ(canvas.calls.back(), std::string{"polygon 3"});

        canvas.draw(sc::circle{{5, 5}, 3});
        canvas.draw(sc::circle_i{{5, 5}, 4});
        CHECK_EQ(canvas.calls.back(), std::string{"circle 4"});

        canvas.draw(sc::text{"Hello", {10, 20}, {"serif", 14}});
        CHECK_EQ(canvas.calls.back(), std::string{"text Hello serif"});
        canvas.draw(sc::line{{0, 0}, {10, 10}}, sc::stroke{sc::color::Black, 3});
        CHECK_EQ(canvas.calls.back(), std::string{"line 3"});
        canvas.draw(sc::line_i{{0, 0}, {10, 10}});
        CHECK_EQ(canvas.calls.back(), std::string{"line 1"});
        for (const auto &segment: std::vector<sc::line>{{{0, 0}, {1, 1}}, {{1, 1}, {2, 0}}}) canvas.draw(segment, sc::stroke{sc::color::Red, 2});
        CHECK_EQ(canvas.calls.back(), std::string{"line 2"});
    }

    SECTION("Rounded corners");
    {
        recording_canvas canvas;
        canvas.draw(sc::rounded_rect{sc::rect{0, 0, 100, 50}, 10});
        CHECK_EQ(canvas.calls.back(), std::string{"polygon 36"}); // four arcs of 8 segments
        const auto bounds = static_cast<sc::rect>(canvas.last_polygon);
        CHECK_NEAR(bounds.left(), 0.0, 1e-9);
        CHECK_NEAR(bounds.bottom(), 50.0, 1e-9);
        CHECK_NEAR(canvas.last_polygon[0].x(), 0.0, 1e-9); // starts on the left edge, 10 down
        CHECK_NEAR(canvas.last_polygon[0].y(), 10.0, 1e-9);
        canvas.draw(sc::rounded_rect{sc::rect{0, 0, 100, 50}, 500}); // clamped to 25
        const auto clamped = static_cast<sc::rect>(canvas.last_polygon);
        CHECK_NEAR(clamped.height(), 50.0, 1e-9);
        canvas.draw(sc::rounded_rect_i{sc::rect_i{0, 0, 100, 50}, 0}); // square corners: a plain rectangle
        CHECK_EQ(canvas.calls.back(), std::string{"polygon 4"});
    }

    SECTION("What draws nothing");
    {
        recording_canvas canvas;
        canvas.draw(sc::rect{0, 0, 10, 10}, sc::color::Transparent);                 // no stroke, no fill
        canvas.draw(sc::rect{0, 0, 10, 10}, sc::stroke::none());
        canvas.draw(sc::rect{0, 0, 10, 10}, sc::stroke{sc::color::Black, 0});        // no width
        canvas.draw(sc::polygon{{1, 1}});
        canvas.draw(sc::circle{{1, 1}, 0});
        canvas.draw(sc::text{"", {0, 0}});
        canvas.draw(sc::text{"x", {0, 0}}, sc::color::Transparent);
        canvas.draw(sc::line{{0, 0}, {1, 1}}, sc::stroke{sc::color::Black, 0});
        CHECK(canvas.calls.empty());

        canvas.draw(sc::rect{0, 0, 10, 10}, {sc::stroke{sc::color::Black, 0}, sc::color::Red}); // fill only
        CHECK(canvas.last_stroke == sc::stroke::none()); // passed on as none
        CHECK(canvas.last_fill.colour() == sc::color::Red);
    }

    SECTION("Measuring text");
    {
        recording_canvas canvas;
        CHECK_EQ(canvas.text_width(sc::text{"abcd", {5, 5}, {"serif", 10}}), 20.0);
        CHECK_EQ(canvas.text_width(sc::text{}), 0.0);                           // nothing to measure
        CHECK_EQ(canvas.text_width(sc::text{"abcd", {}, {"serif", 0}}), 0.0);
        CHECK_EQ(canvas.text_x_height(), 6.0);                                  // the default font, 12
        CHECK_NEAR(canvas.text_cap_height({"serif", 20}), 14.0, 1e-12);
    }

    SECTION("Paths and SVG");
    {
        recording_canvas canvas;
        sc::path triangle;
        triangle.move_to({0, 0}).line_to({10, 0}).line_to({5, 8}).close();
        canvas.draw(triangle, {sc::color::Black, sc::color::Red});
        CHECK_EQ(canvas.calls.back(), std::string{"polygon 3"}); // by default, a closed subpath is a polygon

        sc::path open;
        open.move_to({0, 0}).cubic_to({0, 10}, {10, 10}, {10, 0});
        canvas.calls.clear();
        canvas.draw(open, sc::stroke{sc::color::Black, 2});
        CHECK_LT(size_t{3}, canvas.calls.size()); // an open curve: short lines
        CHECK_EQ(canvas.calls.back(), std::string{"line 2"});

        const auto badge = sc::drawing::from_svg(R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="200" height="100">)svg"
                                                 R"svg(<rect width="200" height="100" fill="#ff0000"/></svg>)svg");
        canvas.calls.clear();
        canvas.draw(badge, sc::rect{0, 0, 100, 100}); // half size, centred vertically
        CHECK_EQ(canvas.calls.size(), size_t{1});
        const auto bounds = static_cast<sc::rect>(canvas.last_polygon);
        CHECK_NEAR(bounds.left(), 0.0, 1e-9);
        CHECK_NEAR(bounds.top(), 25.0, 1e-9);
        CHECK_NEAR(bounds.width(), 100.0, 1e-9);
        CHECK_NEAR(bounds.height(), 50.0, 1e-9);
        CHECK(canvas.last_fill.colour() == sc::color::Red);

        const auto clipped = sc::drawing::from_svg(R"svg(<svg xmlns="http://www.w3.org/2000/svg" width="10" height="10">)svg"
                                                   R"svg(<defs><clipPath id="c"><rect width="5" height="5"/></clipPath></defs>)svg"
                                                   R"svg(<rect clip-path="url(#c)" width="10" height="10" fill="#000000"/></svg>)svg");
        canvas.calls.clear();
        canvas.draw(clipped, sc::rect{0, 0, 10, 10});
        CHECK(!canvas.calls.empty() && canvas.calls.front().starts_with("clip"));
        CHECK(!canvas.calls.empty() && canvas.calls.back() == "unclip");
    }

    SECTION("Any geometry: a shape of its own, and sc::shape");
    {
        recording_canvas canvas;
        canvas.draw(diamond{{50, 50}, 10}, {sc::color::Black, sc::color::Red}); // drawn as its path
        CHECK_EQ(canvas.calls.back(), std::string{"polygon 4"});
        CHECK(canvas.last_fill.colour() == sc::color::Red);
        CHECK_NEAR(canvas.last_polygon[0].y(), 40.0, 1e-9);

        const std::vector<sc::shape> marks{sc::rect{0, 0, 5, 5}, sc::circle{{9, 9}, 2}, sc::line{{0, 0}, {3, 3}}};
        canvas.calls.clear();
        for (const auto &mark: marks) canvas.draw(mark, {sc::stroke{sc::color::Blue, 2}, sc::color::Green});
        CHECK_EQ(canvas.calls, (std::vector<std::string>{"polygon 4", "circle 2", "line 2"})); // each as itself
    }

    SECTION("A group: each of its shapes as itself, nested groups too");
    {
        recording_canvas canvas;
        sc::group marks{sc::rect{0, 0, 5, 5}, sc::circle{{9, 9}, 2}, sc::line{{0, 0}, {3, 3}}};
        marks.push_back(sc::group{sc::circle{{20, 20}, 3}});
        canvas.draw(marks, {sc::stroke{sc::color::Blue, 2}, sc::color::Green});
        CHECK_EQ(canvas.calls, (std::vector<std::string>{"polygon 4", "circle 2", "line 2", "circle 3"}));
        canvas.calls.clear();
        canvas.draw(sc::shape{marks}); // through sc::shape: the same
        CHECK_EQ(canvas.calls.size(), std::size_t{4});
        canvas.calls.clear();
        marks.flip_horizontal(); // still a rect and circles after the flip
        canvas.draw(marks);
        CHECK_EQ(canvas.calls, (std::vector<std::string>{"polygon 4", "circle 2", "line 1", "circle 3"}));
    }

    SECTION("Fonts");
    {
        const sc::font plain;
        CHECK_EQ(plain.family, std::string{"sans-serif"});
        CHECK_EQ(plain.size, 12.0);
        CHECK_EQ(plain.weight, 400);
        CHECK_EQ(sc::font::bold("serif", 20).weight, 700);
        const sc::font small{"serif"}, large{"serif", 14};
        CHECK(small < large);
    }
    TEST_SUMMARY();
}
