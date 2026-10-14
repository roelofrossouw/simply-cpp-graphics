#ifndef PARSESVG_SVGPARSER_H
#define PARSESVG_SVGPARSER_H
#include <memory>
#include <string_view>
#include "svg_element.h"

namespace sc {
    struct svg_parser_impl;

    class svg_processor {
    public:
        virtual ~svg_processor() = default;

        virtual svg_element *root() = 0;
    };

    class svg_parser {
    public:
        explicit svg_parser(std::string_view xml_data);

        ~svg_parser();

        svg_parser(const svg_parser &) = delete;

        svg_parser &operator=(const svg_parser &) = delete;

        void process(svg_processor *container);

    private:
        std::unique_ptr<svg_parser_impl> impl;
    };
}

#endif //PARSESVG_SVGPARSER_H
