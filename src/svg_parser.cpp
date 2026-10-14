#include "svg_parser.h"

#include "svg_element.h"

#include <tinyxml2.h>

#include <map>
#include <stdexcept>
#include <string>

namespace sc {
    struct svg_parser_impl {
        tinyxml2::XMLDocument doc;
    };

    namespace {
        std::map<std::string, std::string> attributes(const tinyxml2::XMLElement *element) {
            std::map<std::string, std::string> attributes;
            auto attr = element->FirstAttribute();
            while (attr) {
                attributes[attr->Name()] = attr->Value();
                attr = attr->Next();
            }
            return attributes;
        }

        void walk(tinyxml2::XMLElement *element, svg_processor *container, svg_element *parent = nullptr) {
            if (!element) return;
            if (!parent) parent = container->root();
            const auto svg = svg_element::generator(parent, element->Name(), attributes(element));
            walk(element->FirstChildElement(), container, svg);
            walk(element->NextSiblingElement(), container, parent);
        }
    }


    svg_parser::svg_parser(const std::string_view xml_data) : impl(std::make_unique<svg_parser_impl>()) {
        impl->doc.Parse(xml_data.data(), xml_data.size());
        std::string problem;
        if (impl->doc.Error()) problem = std::string{"sc::svg: not valid SVG: "} + impl->doc.ErrorStr();
        else if (!impl->doc.RootElement() || std::string_view{impl->doc.RootElement()->Name()} != "svg")
            problem = "sc::svg: not SVG: the root element isn't <svg>";
        if (!problem.empty()) throw std::runtime_error(problem);
    }

    svg_parser::~svg_parser() = default;

    void svg_parser::process(svg_processor *container) {
        walk(impl->doc.RootElement(), container);
        container->root()->do_transform();
        container->root()->normalize();
    }
}
