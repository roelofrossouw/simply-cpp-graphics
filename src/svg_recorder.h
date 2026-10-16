#ifndef SC_SVG_RECORDER_H
#define SC_SVG_RECORDER_H

#include "svg.h"

#include <optional>
#include <vector>

namespace sc {
    // What svg_element draws into: the handful of PDF-style drawing operators it uses, recorded as
    // sc::svg elements (a path with its fill or stroke) instead of being written anywhere.
    class svg_recorder {
    public:
        explicit svg_recorder(std::vector<svg::element> &elements) : elements(elements) {
        }

        // Graphics state: saved and restored, as PDF's q and Q.
        void q() { states.push_back(states.back()); }

        void Q() {
            if (states.size() > 1) states.pop_back();
        }

        // Path construction.
        void m(const double x, const double y) { current.move_to({x, y}); }
        void l(const double x, const double y) { current.line_to({x, y}); }

        void c(const double x1, const double y1, const double x2, const double y2, const double x, const double y) {
            current.cubic_to({x1, y1}, {x2, y2}, {x, y});
        }

        void h() { current.close(); }

        void re(const double x, const double y, const double width, const double height) {
            current.move_to({x, y}).line_to({x + width, y}).line_to({x + width, y + height}).line_to({x, y + height}).close();
        }

        // Colours (the element's own, alpha included) and line style; joins, caps and the miter
        // limit aren't kept.
        void rg(const color &fill) { states.back().fill = fill; }
        void RG(const color &stroke) { states.back().stroke = stroke; }
        void w(const double width) { states.back().width = width; }
        void j(int) {}
        void J(int) {}
        void M(double) {}

        // Painting: each ends the current path.
        void f() { emit(true, false); }

        void fStar() { emit(true, false, fill_rule::even_odd); }

        void S() { emit(false, true); }

        void s() {
            current.close();
            emit(false, true);
        }

        // Clipping: W marks the current path as the clip for what follows until the Q, n ends it.
        void W() { clip_next = true; }

        void n() {
            if (clip_next) states.back().clip = current;
            clip_next = false;
            current = {};
        }

    private:
        struct state {
            color fill = color::Black;
            color stroke = color::Black;
            double width = 1;
            std::optional<path> clip;
        };

        std::vector<svg::element> &elements;
        std::vector<state> states{state{}};
        path current;
        bool clip_next = false;

        void emit(const bool filled, const bool stroked, const fill_rule rule = fill_rule::nonzero) {
            const auto &now = states.back();
            if (!current.empty())
                elements.push_back({current, stroked ? sc::stroke{now.stroke, now.width} : sc::stroke::none(),
                                    filled ? sc::fill{now.fill, rule} : sc::fill::none(), now.clip});
            current = {};
        }
    };
}

#endif //SC_SVG_RECORDER_H
