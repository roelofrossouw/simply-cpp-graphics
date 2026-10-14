# simply-cpp-graphics

C++20 drawing interface for simply-cpp: `sc::canvas`, with `sc::text` and
`sc::font`, `sc::path` (lines and curves) and `sc::svg`. It draws nothing
itself: engines implement it, so the same calls draw on any of them.

| Engine | Module | Draws with |
|---|---|---|
| `sc::image` | [sc-image](https://github.com/roelofrossouw/simply-cpp-image) | OpenCV, in pixels |
| `sc::pdf` | [sc-pdf](https://github.com/roelofrossouw/simply-cpp-pdf) | PDFHummus, in points, as vectors |

**Documentation:** the [simply-cpp wiki](https://github.com/roelofrossouw/simply-cpp/wiki) has a [Graphics guide](https://github.com/roelofrossouw/simply-cpp/wiki/Graphics) and the [sc-graphics reference](https://github.com/roelofrossouw/simply-cpp/wiki/Reference-sc-graphics), plus [getting started](https://github.com/roelofrossouw/simply-cpp/wiki/Getting-Started) for the whole suite.

Shapes are sc-core's geometry (`rect`, `rotated_rect`, `polygon`, `circle`,
`line`, each as double or int) and colours are `sc::color`s. Coordinates are
the canvas's own, from the top-left corner with y down.

```cpp
#include <canvas.h>

void label(sc::canvas &on, const sc::rect &box, const std::string &name) {
    on.draw(box);                                                   // black outline, 1 wide
    on.draw(box, sc::color::Black, sc::color{0.9, 0.95, 1}, 1, 4);  // stroke, fill, width, corner radius
    on.draw(sc::circle{box.center(), 3}, sc::color::Transparent, sc::color::Red); // filled only
    on.draw(sc::line{box.left_top(), box.right_bottom()}, sc::color::Blue, 0.5);  // lines have no fill

    sc::text caption{name, {box.left(), box.bottom() + 14}, {"sans-serif", 10}};
    caption.position.x(box.center().x() - on.text_width(caption) / 2);           // centred under it
    on.draw(caption);

    on.draw(sc::svg::load("logo.svg"), box);                        // fitted into box, centred
}
```

Everything goes through `draw(shape, stroke = black, fill = transparent,
width = 1)`; a transparent colour leaves the stroke or fill out.
`draw(text, colour)` writes a line of text with its baseline at
`text.position`, and `text_width(text)`, `text_x_height(font)` and
`text_cap_height(font)` measure it. `sc::svg` reads SVG text or a file into
paths with their colours (paths, the basic shapes, groups, transforms, clip
paths, `<defs>`/`<use>`, flat colours: icons and logos, not gradients, text or
filters).

## Writing an engine

A canvas implements a few protected primitives; `draw()` reduces every shape
to them, after leaving out invisible strokes and fills and empty shapes:

- `paint_polygon`, `paint_circle`, `paint_line`, `paint_text` (required);
- `measure_width`, `measure_x_height`, `measure_cap_height` (required);
- `paint_rounded_rect` and `paint_path`, which by default draw straight
  segments, and `push_clip`/`pop_clip`, which by default don't clip, for an
  engine that can do better.

## Demo

`sc-graphics-demo` is such an engine: a canvas that only prints what it's asked
to paint, showing what each call becomes. It's installed with the runtime
package (`simply-cpp-graphics`). The engine, copied from
`examples/sc-graphics-demo.cpp` at configure time:

<!-- sc-example: examples/sc-graphics-demo.cpp -->
```cpp
// A canvas implements the primitives; canvas::draw() does the rest.
class printing_canvas : public sc::canvas {
protected:
    void paint_polygon(const sc::polygon &shape, const sc::color &stroke, const sc::color &fill, double width) override {
        print("polygon of " + std::to_string(shape.size()) + " points", stroke, fill, width);
    }

    void paint_circle(const sc::circle &shape, const sc::color &stroke, const sc::color &fill, double width) override {
        print("circle, radius " + sc::console::format(shape.radius()), stroke, fill, width);
    }

    void paint_line(const sc::point &from, const sc::point &to, const sc::color &colour, double width) override {
        print("line " + sc::console::format(from) + " to " + sc::console::format(to), colour, sc::color::Transparent, width);
    }

    void paint_text(const sc::text &sample, const sc::color &colour) override {
        sc::console::note("text \"" + sample.content + "\" in " + sample.font.family + " " + sc::console::format(sample.font.size) +
                          ", " + sc::console::format(colour));
    }

    double measure_width(const sc::text &sample) override { return sample.content.size() * sample.font.size / 2; }
    double measure_x_height(const sc::font &font) override { return font.size / 2; }
    double measure_cap_height(const sc::font &font) override { return font.size * 0.7; }

private:
    static void print(const std::string &what, const sc::color &stroke, const sc::color &fill, const double width) {
        std::string how;
        if (width > 0) how += " outlined " + sc::console::format(stroke) + ", " + sc::console::format(width) + " wide";
        if (fill.alpha() > 0) how += " filled " + sc::console::format(fill);
        sc::console::note(what + how);
    }
};
```
<!-- /sc-example -->

## Building locally

Run `bash scripts/install.sh` to configure, build, test, package, and install.
Depends on sc-core and tinyxml2 (for reading SVG).

## Usage

```cmake
find_package(sc-graphics CONFIG REQUIRED)

target_link_libraries(myapp PRIVATE sc::sc-graphics-shared)
```

```bash
sudo curl -fsSL https://apt.roelof.co.za/setup.sh | bash
sudo apt -y install simply-cpp-graphics-dev
```

Usually you get it with an engine: `simply-cpp-image-dev` and
`simply-cpp-pdf-dev` depend on it.
