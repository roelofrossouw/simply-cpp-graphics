# simply-cpp-graphics

C++20 drawing interface for simply-cpp: `sc::graphics::canvas`, `sc::graphics::appearance`
(stroke and fill), text and fonts, `sc::graphics::drawing` (pictures as data, read and
written as SVG) and page sizes. It draws nothing itself: engines implement it,
so the same calls draw on any of them.

| Engine | Module | Draws with | Partly transparent colours |
|---|---|---|---|
| `sc::image::picture` | [sc-image](https://github.com/roelofrossouw/simply-cpp-image) | OpenCV, in pixels | painted opaque |
| `sc::pdflite::writer` | [sc-pdflite](https://github.com/roelofrossouw/simply-cpp-pdflite) | sc-pdflite, in points, as vectors (Apache-2.0) | as they are |
| `sc::mupdf::writer` | [sc-mupdf](https://github.com/roelofrossouw/simply-cpp-mupdf) | MuPDF, in points, as vectors (AGPL-3.0) | as they are |

**Documentation:** the [simply-cpp wiki](https://github.com/roelofrossouw/simply-cpp/wiki) has a [Graphics guide](https://github.com/roelofrossouw/simply-cpp/wiki/Graphics) and the [sc-graphics reference](https://github.com/roelofrossouw/simply-cpp/wiki/Reference-sc-graphics), plus [getting started](https://github.com/roelofrossouw/simply-cpp/wiki/Getting-Started) for the whole suite.

## Drawing

Shapes are sc-core's geometry (`rect`, `rounded_rect`, `rotated_rect`,
`polygon`, `circle`, `line`, as double or int, and `path`). Any other
`sc::geometry` draws through its path, an `sc::shape` as the shape it holds,
and an `sc::group` shape by shape. Coordinates are the canvas's own, from the
top-left corner, y down.

```cpp
#include <sc/graphics/canvas.h>

void label(sc::graphics::canvas &on, const sc::rect &box, const std::string &name) {
    on.draw(box);                                                       // black outline, 1 wide
    on.draw(sc::rounded_rect{box, 4}, {sc::color::Black, sc::color{0.9, 0.95, 1}}); // stroke, fill
    on.draw(sc::circle{box.center(), 3}, sc::graphics::appearance::filled(sc::color::Red));     // filled only
    on.draw(sc::line{box.left_top(), box.right_bottom()}, sc::graphics::stroke{sc::color::Blue, 0.5}); // no fill

    sc::graphics::text caption{name, {box.left(), box.bottom() + 14}, {"sans-serif", 10}};
    caption.position.x(box.center().x() - on.text_width(caption) / 2);           // centred under it
    on.draw(caption);

    on.draw(sc::graphics::drawing::load_svg("logo.svg"), box);                // fitted into box, centred
    on.draw(sc::graphics::bitmap::load("photo.jpg"), box);                    // stretched to fill box

    on.push_clip(sc::to_path(sc::circle{box.center(), 20}));         // only inside the circle
    on.draw(sc::graphics::bitmap::load("face.png"), box);
    on.pop_clip();
}
```

An `sc::graphics::bitmap` is a PNG or JPEG kept as its file (`bitmap::load(file)`,
`sc::graphics::bitmap{bytes}` or `bitmap::from_data_uri(uri)`), its size read from its
header: PDFs embed it as it is, an `sc::image::picture` decodes it, an `sc::graphics::drawing`
keeps it as a figure (an SVG `<image>` with a data: URI). `push_clip()` and
`pop_clip()` limit what's drawn to inside a path until popped; clips nest.

An `sc::graphics::appearance` is an `sc::graphics::stroke` (colour and width; black, 1 wide by
default) and an `sc::graphics::fill` (colour and fill rule; none by default). A colour
or stroke converts to an outline only, `{stroke, fill}` gives both, and
`sc::graphics::appearance::filled(fill)` the inside only. Transparency is the colour's
alpha; 0 leaves a part out. A line takes a stroke, text a fill (black by
default), with its baseline at `text.position`; `text_width()`,
`text_x_height()` and `text_cap_height()` measure it. A fill can be an
`sc::graphics::linear_gradient` (stops along a line; sc-image draws it, the PDF engines
its middle colour for now). Dashes, caps and joins, radial gradients and
patterns are planned in `paint.h`.

## Drawings and SVG

An `sc::graphics::drawing` (`drawing.h`, included by `canvas.h`) is a page size and its
items in order: `sc::graphics::drawable`s (shape, appearance) and `sc::graphics::label`s (text,
fill), each optionally clipped.

```cpp
sc::graphics::drawing badge{{200, 100}};
badge.add(sc::rounded_rect{sc::rect{0, 0, 200, 100}, 12}, sc::graphics::appearance::filled(sc::color{"steelblue"}));
badge.add(sc::graphics::text{"simply-cpp", {20, 60}, sc::graphics::font::bold("sans-serif", 24)}, sc::color::White);
badge.bounds();                                // what's on it
badge.flip_horizontal();                       // mirrored across its page
badge.apply(sc::transform::scale(0.5));        // outlines and text scale with it
pdf.draw(badge, sc::rect{50, 50, 200, 100});   // fitted into the box, centred
badge.save_svg("badge.svg");                   // or to_svg()

const auto logo = sc::graphics::drawing::load_svg("logo.svg");   // or from_svg(text)
```

Reading SVG covers paths, basic shapes, text, groups, transforms, clip paths,
`<defs>`/`<use>`, flat colours with opacity and `style="..."`. Shapes keep
their kind where their transform allows (a moved `<rect>` stays an `sc::rect`,
a turned one becomes an `sc::rotated_rect`); a mask is read as a clip of its
shapes; percentages (`width="100%"`) are of the viewport, the viewBox or else
the width and height, as SVG has them; linear gradients are read with their
stops, units and transform. Not
read: radial gradients (their middle colour is), patterns, markers, filters
and stylesheets. Writing gives each shape its own element, so a
drawing reads back as it was (a group as its shapes).

A drawing is a canvas too: drawing on it records the primitives (a rect as a
polygon), so a page drawn once can be measured, flipped or replayed onto a PDF
or an image.

## Fonts

Installed fonts, found through fontconfig and read with FreeType:

| | What it is | Example |
|---|---|---|
| `sc::graphics::typeface` (`typeface.h`) | A family, with every style installed of it | DejaVu Sans |
| `sc::graphics::font_face` (`font_face.h`) | One style of it, in one file | DejaVu Sans Bold, `DejaVuSans-Bold.ttf` |
| `sc::graphics::font` (`font.h`) | What text is drawn in: family, size, weight, italic, style | DejaVu Sans Bold at 12 pt |

A font's measurements are its closest installed face at its size, as sc-pdflite
and sc-mupdf draw it; `available()` tells whether its family is installed
rather than substituted.

**Characters a font lacks** are drawn, and measured, in an installed face that
has them, as browsers do: text is split into runs by face, so `✓` or `€` in a
font without them still shows. **Emoji** (👍, 🇿🇦, 👩‍👩‍👧, 👍🏽) go to a colour emoji
font (Noto Color Emoji, Apple Color Emoji, Segoe UI Emoji), shaped with
HarfBuzz so flags, skin tones and joined sequences are one glyph each, and are
drawn as bitmaps (rendered once, at 128 pixels) on any canvas, in their own
colours. A VS15 (`❤︎`) asks for the text form. The apt package depends on
`fonts-noto-color-emoji`. Emoji are pictures in a PDF, so they aren't found by
searching its text.

```cpp
#include <sc/graphics/font.h>

const sc::graphics::font body{"serif", 11};
sc::graphics::font{"Roboto", 11}.available();     // false where Roboto isn't installed: it'd be substituted
body.width("Total due");                // the glyphs' advances, no kerning, in points at 11
body.ascender(); body.descender(); body.line_height(); body.x_height(); body.cap_height();
const auto face = body.face();          // DejaVu Serif Book on Ubuntu: its file and index, weight 400
face->typeface().faces();               // every style of the family: Bold, Italic, Condensed, ...
face->typeface().face(700);             // the closest to bold
sc::graphics::typeface::installed();              // every family on the machine
sc::graphics::typeface::add_folder("/srv/fonts"); // fonts kept with an application, for this process only
```

`typeface::add_folder()` adds a folder's fonts to those found, as if installed (fontconfig's
application fonts): call it before anything looks a font up.

### PDF's built-in fonts

`sc::graphics::standard_font` (`standard_font.h`) is one of the fonts every PDF reader has, Helvetica, Times
and Courier in their four styles, with Adobe's metrics (the Core 14 AFM files): widths for the
Windows-1252 characters they're written in, ascender, descender, cap and x heights. sc-pdflite writes
them without embedding anything, and sc-litehtml lays text out with these metrics.

```cpp
#include <sc/graphics/standard_font.h>

const auto font = sc::graphics::standard_font::of(sc::graphics::font{"Arial", 12, 700});   // Helvetica-Bold
font->width("Total", 12);                                              // as every reader draws it
sc::graphics::standard_font::encode(U'€');                                       // 0x80, Windows-1252
```

## Paragraphs

```cpp
#include <sc/graphics/canvas.h>

const auto rest = canvas.draw(sc::graphics::paragraph{text, {50, 100, 495, 600}, {"serif", 11}, sc::graphics::align::justify});
if (!rest.empty()) { /* what didn't fit: on the next page */ }
const auto lines = sc::graphics::wrap(text, sc::graphics::font{"serif", 11}, 495);   // just the lines
```

Lines break at spaces and newlines (a word wider than the line is split), are
aligned left, right, centred or justified (all but a paragraph's last), and
are `line_spacing` x the size apart, the first baseline the font's ascender
below the top. `canvas.draw(paragraph)` measures with that canvas's
`text_width()`, so lines fit as that engine draws them, and returns the text
that didn't fit. No kerning, as the engines draw none.

## Page sizes

```cpp
#include <sc/graphics/page.h>

sc::graphics::page_dimensions(sc::graphics::page_size::A4);                                // 210 x 297 mm: 595.28 x 841.89 points
sc::graphics::page_dimensions(sc::graphics::page_size::A3, sc::graphics::orientation::landscape);    // 420 x 297 mm
sc::graphics::page_size_from("Letter");                                         // any case; else std::invalid_argument
```

Sizes are `A3`, `A4`, `A5`, `letter` and `legal`.

## Writing an engine

A canvas implements a few protected primitives; `draw()` reduces every shape
to them, leaving out empty shapes and invisible strokes and fills (passed as
`none()`):

- required: `paint_polygon`, `paint_circle` (an `sc::graphics::appearance`),
  `paint_line` (an `sc::graphics::stroke`), `paint_text` (an `sc::graphics::fill`),
  `paint_bitmap` (a bitmap and the transform placing its unit square), and
  `measure_width`, `measure_x_height`, `measure_cap_height`;
- optional: `paint_rounded_rect` and `paint_path`, which default to straight
  segments, and `push_clip`/`pop_clip`, which default to not clipping.

## Demo

`sc-graphics-demo`, installed with the runtime package
(`simply-cpp-graphics`), is such an engine: a canvas that prints what it's
asked to paint. Its engine, copied from `examples/sc-graphics-demo.cpp` at
configure time:

<!-- sc-example: examples/sc-graphics-demo.cpp -->
```cpp
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
```
<!-- /sc-example -->

## Building locally

Run `bash scripts/install.sh` to configure, build, test, package, and install.
Depends on sc-core, sc-web (reading SVG as XML, on libxml2), fontconfig and FreeType.

## Usage

```cmake
find_package(sc-graphics CONFIG REQUIRED)

target_link_libraries(myapp PRIVATE sc::sc-graphics-shared)
```

```bash
sudo curl -fsSL https://apt.roelof.co.za/setup.sh | bash
sudo apt -y install simply-cpp-graphics-dev
```

Usually you get it with an engine: `simply-cpp-image-dev`,
`simply-cpp-pdflite-dev` and `simply-cpp-mupdf-dev` depend on it.
