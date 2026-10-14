#include "svg.h"

#include "svg_element.h"
#include "svg_parser.h"
#include "svg_recorder.h"

#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>

namespace sc {
    namespace {
        // The tree svg_parser builds, under one root.
        struct tree : svg_processor {
            std::unique_ptr<svg_element> top = std::make_unique<svg_xml>();

            svg_element *root() override { return top.get(); }
        };
    }

    svg::svg(const std::string_view text) {
        tree drawing;
        svg_parser{text}.process(&drawing);
        width_ = drawing.top->width();
        height_ = drawing.top->height();
        svg_recorder recorder{elements_};
        drawing.top->draw(&recorder);
    }

    svg svg::load(const std::filesystem::path &file) {
        std::ifstream in{file, std::ios::binary};
        if (!in) throw std::runtime_error("sc::svg: can't read " + file.string());
        const std::string text{std::istreambuf_iterator<char>{in}, {}};
        return svg{text};
    }
}
