# simply-cpp-graphics

C++20 drawing interface for simply-cpp: `sc::canvas`, with `sc::stroke` and
`sc::fill`, `sc::text` and `sc::font`, `sc::path` (lines and curves) and
`sc::svg`. It draws nothing
itself: engines implement it, so the same calls draw on any of them.

| Engine | Module | Draws with |
|---|---|---|
| `sc::image` | [sc-image](https://github.com/roelofrossouw/simply-cpp-image) | OpenCV, in pixels |
| `sc::pdf` | [sc-pdf](https://github.com/roelofrossouw/simply-cpp-pdf) | PDFHummus, in points, as vectors (Apache-2.0) |
| `sc::mupdf::writer` | [sc-mupdf](https://github.com/roelofrossouw/simply-cpp-mupdf) | MuPDF, in points, as vectors (AGPL-3.0) |

**Documentation:** the [simply-cpp wiki](https://github.com/roelofrossouw/simply-cpp/wiki) has a [Graphics guide](https://github.com/roelofrossouw/simply-cpp/wiki/Graphics) and the [sc-graphics reference](https://github.com/roelofrossouw/simply-cpp/wiki/Reference-sc-graphics), plus [getting started](https://github.com/roelofrossouw/simply-cpp/wiki/Getting-Started) for the whole suite.

Shapes are sc-core's geometry (`rect`, `rotated_rect`, `polygon`, `circle`,
`line`, each as double or int) and colours are `sc::color`s. Coordinates are
the canvas's own, from the top-left corner with y down.

```cpp
#include <canvas.h>

void label(sc::canvas &on, const sc::rect &box, const std::string &name) {
    on.draw(box);                                                       // black outline, 1 wide
    on.draw(sc::rounded_rect{box, 4}, sc::color::Black, sc::color{0.9, 0.95, 1}); // stroke, fill
    on.draw(sc::circle{box.center(), 3}, sc::stroke::none(), sc::color::Red);     // filled only
    on.draw(sc::line{box.left_top(), box.right_bottom()}, sc::stroke{sc::color::Blue, 0.5}); // no fill

    sc::text caption{name, {box.left(), box.bottom() + 14}, {"sans-serif", 10}};
    caption.position.x(box.center().x() - on.text_width(caption) / 2);           // centred under it
    on.draw(caption);

    on.draw(sc::svg::load("logo.svg"), box);                        // fitted into box, centred
}
```

Everything goes through `draw(shape, stroke, fill)`: an `sc::stroke` (a colour
and a width; black and 1 wide by default) for the outline and an `sc::fill` (a
colour; none by default) for the inside. A colour converts to either, and
`sc::stroke::none()` or a transparent colour leaves a part out. Rounded corners
belong to the shape, `sc::rounded_rect{rect, radius}`; a line has no inside,
`draw(line, stroke)`. Dashes, caps and joins, nibs, gradients and patterns are
planned in `paint.h`; each engine draws what it supports and ignores the rest.
`draw(text, fill)` writes a line of text (black by default) with its baseline at
`text.position`, and `text_width(text)`, `text_x_height(font)` and
`text_cap_height(font)` measure it. `sc::svg` reads SVG text or a file into
paths with their colours (paths, the basic shapes, groups, transforms, clip
paths, `<defs>`/`<use>`, flat colours: icons and logos, not gradients, text or
filters).

## Fonts: typefaces, faces and fonts

Installed fonts come in three levels, found through fontconfig and read with
FreeType:

| | What it is | Example |
|---|---|---|
| `sc::typeface` (`typeface.h`) | A family, with every style installed of it, usually in several files | DejaVu Sans |
| `sc::font_face` (`font_face.h`) | One style of it, in one file | DejaVu Sans Bold, `DejaVuSans-Bold.ttf` |
| `sc::font` (`font.h`) | A face at a size: what text is drawn and measured in | DejaVu Sans Bold at 12 pt |

`sc::font` stays what you ask for (family, size, weight, italic, style).
`face()` finds the installed face closest to it, `available()` tells whether
its family is installed rather than substituted, and the measurements are
that face at the font's size: how sc-pdf and sc-mupdf, which use the same
files, draw it.

```cpp
#include <font.h>

const sc::font body{"serif", 11};
sc::font{"Roboto", 11}.available();     // false where Roboto isn't installed: it'd be substituted
body.width("Total due");                // the glyphs' advances, no kerning, in points at 11
body.ascender(); body.descender(); body.line_height(); body.x_height(); body.cap_height();
const auto face = body.face();          // DejaVu Serif Book on Ubuntu: its file and index, weight 400
face->typeface().faces();               // every style of the family: Bold, Italic, Condensed, ...
face->typeface().face(700);             // the closest to bold
sc::typeface::installed();              // every family on the machine
```

## Paragraphs: wrapping text in a rectangle

```cpp
#include <canvas.h>

const auto rest = canvas.draw(sc::paragraph{text, {50, 100, 495, 600}, {"serif", 11}, sc::align::justify});
if (!rest.empty()) { /* what didn't fit: on the next page */ }
const auto lines = sc::wrap(text, sc::font{"serif", 11}, 495);   // just the lines
```

A paragraph breaks at spaces and newlines, and splits a word wider than the
line. Its lines are aligned left, right, centred or justified (every line of
a paragraph but its last), `line_spacing` x the size apart, the first baseline
the font's ascender below the top. `canvas.draw(paragraph)` measures with that
canvas's own `text_width()`, so the lines fit as that engine draws them (sc-image
measures its Hershey fonts), and returns the text that didn't fit the
rectangle's height. There's no kerning, as the engines draw none.

## Writing an engine

A canvas implements a few protected primitives; `draw()` reduces every shape
to them, after leaving out invisible strokes and fills and empty shapes:

- `paint_polygon`, `paint_circle`, `paint_line`, `paint_text` (required), each
  given an `sc::stroke` and/or `sc::fill`, as `none()` when it wouldn't show;
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
    void paint_polygon(const sc::polygon &shape, const sc::stroke &stroke, const sc::fill &fill) override {
        print("polygon of " + std::to_string(shape.size()) + " points", stroke, fill);
    }

    void paint_circle(const sc::circle &shape, const sc::stroke &stroke, const sc::fill &fill) override {
        print("circle, radius " + sc::console::format(shape.radius()), stroke, fill);
    }

    void paint_line(const sc::line &shape, const sc::stroke &stroke) override {
        print("line " + sc::console::format(shape.from()) + " to " + sc::console::format(shape.to()), stroke, sc::fill::none());
    }

    void paint_text(const sc::text &sample, const sc::fill &fill) override {
        sc::console::note("text \"" + sample.content + "\" in " + sample.font.family + " " + sc::console::format(sample.font.size) +
                          ", " + sc::console::format(fill.colour()));
    }

    double measure_width(const sc::text &sample) override { return sample.content.size() * sample.font.size / 2; }
    double measure_x_height(const sc::font &font) override { return font.size / 2; }
    double measure_cap_height(const sc::font &font) override { return font.size * 0.7; }

private:
    static void print(const std::string &what, const sc::stroke &stroke, const sc::fill &fill) {
        std::string how;
        if (stroke.visible()) how += " outlined " + sc::console::format(stroke.colour()) + ", " + sc::console::format(stroke.width()) + " wide";
        if (fill.visible()) how += " filled " + sc::console::format(fill.colour());
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
