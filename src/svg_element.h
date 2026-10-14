#ifndef SC_SVG_ELEMENT_H
#define SC_SVG_ELEMENT_H
#include <iostream>
#include <istream>
#include <map>
#include <vector>
#include <color.h>
#include <rect.h>

namespace sc {
    class svg_recorder;

    struct svg_point {
        double x, y;

        svg_point(const double x = 0, const double y = 0) : x(x), y(y) {
        }

        friend std::ostream &operator<<(std::ostream &lhs, const svg_point &rhs);

        svg_point &operator+=(const svg_point &add);

        svg_point &operator-=(const svg_point &sub);

        svg_point &operator*=(double factor);

        svg_point operator+(const svg_point &add) const;

        svg_point operator-(const svg_point &sub) const;

        void rotate(double angle, svg_point center = {0, 0});

        svg_point operator*(double i) const;

        bool operator==(const svg_point &svg_point) const;
    };

    class svg_element {
    public:
        explicit svg_element(bool relative = false);

        svg_element(const svg_element &other);

        svg_element(const svg_element &other, svg_element *parent);

        virtual ~svg_element();

        color get_fill() const;

        color get_stroke() const;

        double get_stroke_width() const;

        svg_element *add_child(svg_element *child, bool is_svg = false);

        virtual svg_element *add_child(svg_element *child, const std::map<std::string, std::string> &attributes, bool is_svg);

        svg_element *add_child(svg_element *child, const std::vector<double> &values);

        std::string path();

        [[nodiscard]] virtual std::string string() const;

        friend std::ostream &operator<<(std::ostream &lhs, const svg_element &rhs);

        svg_point get_point(int i) const;

        void set_point(int i, double x, double y, bool rel = false);

        void set_point(int i, svg_point pt, bool rel = false);

        double width_value(const std::string &value) const;

        double height_value(const std::string &value) const;

        virtual void draw(svg_recorder *ctx) const;

        virtual void stroke(svg_recorder *ctx) const;

        void virtual scale(double scale_x, double scale_y);

        [[nodiscard]] virtual double width() const;

        [[nodiscard]] virtual double height() const;

        [[nodiscard]] rect virtual get_viewBox() const;

        void set_id(const std::string &value);

        void trim(std::string &name);

        void add_styles(const std::string &style);

        void add_style(const std::string &name, const std::string &value);

        bool add_common_attribute(const std::string &name, const std::string &value);

        void resize(int new_width, int new_height, bool keep_aspect);

        void fetch_clip_path(const std::string &name);

        void start_clip(svg_recorder *ctx) const;

        static void end_clip(svg_recorder *ctx);

        [[nodiscard]] virtual svg_element *clone() const = 0;

        void fetch_named_element(const std::string &name);

        void rotate(double angle, svg_point center = {0, 0});

        /**
        Translate viewport to start at 0,0
         */
        void normalize();

        virtual void normalize(svg_point mv);

        std::string get_id();

        virtual void do_transform();

        virtual void flip();

        svg_element *get_svg() const;

        std::vector<svg_element *> get_children();

        static svg_element *generator(svg_element *parent, const std::string &name, const std::map<std::string, std::string> &attributes = {});

        void set_fill(const std::string &color_spec);

        void set_fill(const color &color_spec);

        void set_stroke(const std::string &color_spec);

        void set_stroke(const color &color_spec);

        void set_stroke_width(double new_width = 1);

        [[nodiscard]] svg_point get_start() const;

        [[nodiscard]] svg_point get_end() const;

        void set_start(double x, double y);

        void set_start_x(double x);

        void set_start_y(double y);

        virtual void set_start(const svg_point &pt);

        void set_end(double x, double y);

        void set_end(const svg_point &pt);

        void set_relative_end(double x, double y);

        void set_relative_end(svg_point &endpoint);

    protected:
        bool relative;

        color fill = color::Black;
        int fill_rule = 0; // 1 for evenodd

        color stroke_color = color::Transparent;
        double stroke_width = 1;
        int stroke_cap = 0;
        int stroke_join = 0;
        double stroke_miter_limit = 10;

        std::string transform;
        std::string id;
        std::vector<svg_point> points; // Start = 0, End = 1, extra data after that.
        std::vector<svg_element *> children;
        svg_element *parent = nullptr, *previous = nullptr, *root = nullptr, *svg = nullptr;
        svg_element *clip_path = nullptr;

        virtual void add_attributes(const std::map<std::string, std::string> &attributes);

        virtual void add_attributes(const std::vector<double> &attributes);

        virtual void add_attribute(const std::string &name, const std::string &value);

        [[nodiscard]] virtual std::string type() const;

        std::vector<double> parse_transform_params(const std::string &s);

        void parse_transform(const std::string &transform);

        void set_fill_type(svg_recorder *ctx) const;

        void do_fill(svg_recorder *ctx) const;

        void set_stroke_type(svg_recorder *ctx) const;
    };

    class svg_xml : public svg_element {
        void add_attribute(const std::string &name, const std::string &value) override;

        [[nodiscard]] std::string type() const override;

    public:
        svg_xml();

        [[nodiscard]] svg_element *clone() const override;
    };

    class svg_header : public svg_element {
    public:
        void set_width(double w);

        void set_height(double h);

        void set_viewBox(const std::vector<double> &values);

        void set_viewBox(double x, double y, double w, double h);

        void add_named_element(svg_element *element);

        svg_element *get_named_element(const std::string &name);

        [[nodiscard]] svg_element *clone() const override;

        void normalize(svg_point mv) override;

    private:
        rect viewBox{};

        std::map<std::string, svg_element *> named_elements;

        void add_attribute(const std::string &name, const std::string &value) override;

        [[nodiscard]] std::string type() const override;

        [[nodiscard]] std::string string() const override;

        void scale(double scale_x, double scale_y) override;

        [[nodiscard]] double width() const override;

        [[nodiscard]] double height() const override;

        [[nodiscard]] rect get_viewBox() const override;
    };

    class svg_group : public svg_element {
        void add_attribute(const std::string &name, const std::string &value) override;

        [[nodiscard]] std::string type() const override;

        [[nodiscard]] std::string string() const override;

        void flip() override;

    public:
        [[nodiscard]] svg_element *clone() const override;
    };

    class svg_path : public svg_element {
        void add_command(char type, const std::vector<double> &values);

        static bool isdecimal(char character);

        bool issign(char character);

        double get_double(std::istringstream &ss);

    public:
        void draw(svg_recorder *ctx) const override;

        void stroke(svg_recorder *ctx) const override;

        [[nodiscard]] svg_element *clone() const override;

        [[nodiscard]] std::string string() const override;

    protected:
        void parse_path(const std::string &value);

        [[nodiscard]] std::string type() const override;

        void add_attribute(const std::string &name, const std::string &value) override;
    };

    class svg_clip_path : public svg_path {
    public:
        void draw(svg_recorder *ctx) const override;

    protected:
        [[nodiscard]] std::string type() const override;
    };


    class svg_move : public svg_element {
    public:
        explicit svg_move(bool is_relative = false) { relative = is_relative; }

        [[nodiscard]] svg_element *clone() const override;

        void stroke(svg_recorder *ctx) const override;

    protected:
        void add_attributes(const std::vector<double> &values) override;

        [[nodiscard]] std::string type() const override;
    };

    class svg_line : public svg_element {
    public:
        explicit svg_line(bool is_relative = false) { relative = is_relative; }

        [[nodiscard]] svg_element *clone() const override;

        void stroke(svg_recorder *ctx) const override;

    protected:
        void add_attributes(const std::vector<double> &values) override;

        [[nodiscard]] std::string type() const override;
    };

    class svg_horizontal_line : public svg_line {
    public:
        explicit svg_horizontal_line(bool is_relative = false) { relative = is_relative; }

    protected:
        void add_attributes(const std::vector<double> &values) override;
    };

    class svg_vertical_line : public svg_line {
    public:
        void add_attributes(const std::vector<double> &values) override;

        explicit svg_vertical_line(bool is_relative = false) { relative = is_relative; }
    };

    class svg_arc : public svg_element {
        svg_point radius;
        double x_axis_rotation = 0;
        bool large_arc_flag = false, sweep_flag = false;

    protected:
        void add_attributes(const std::vector<double> &values) override;

    public:
        [[nodiscard]] std::string type() const override;

        std::string string() const override;

        static double angleBetween(double ux, double uy, double vx, double vy);

        void stroke(svg_recorder *ctx) const override;

        void scale(double scale_x, double scale_y) override;

        explicit svg_arc(bool is_relative = false) { relative = is_relative; }

        [[nodiscard]] svg_element *clone() const override;

        void flip() override;
    };

    class svg_close : public svg_element {
    protected:
        void add_attributes(const std::vector<double> &values) override;

        [[nodiscard]] std::string type() const override;

    public:
        void stroke(svg_recorder *ctx) const override;

        [[nodiscard]] svg_element *clone() const override;
    };

    class svg_cubic_bezier : public svg_element {
    protected:
        void add_attributes(const std::vector<double> &values) override;

        [[nodiscard]] std::string type() const override;

    public:
        explicit svg_cubic_bezier(bool is_relative = false) { relative = is_relative; }

        void stroke(svg_recorder *ctx) const override;

        [[nodiscard]] svg_element *clone() const override;
    };

    class svg_quadratic_curve : public svg_element {
    public:
        void stroke(svg_recorder *ctx) const override;

        explicit svg_quadratic_curve(bool is_relative = false) { relative = is_relative; }

        [[nodiscard]] svg_element *clone() const override;

    protected:
        void add_attributes(const std::vector<double> &values) override;

        [[nodiscard]] std::string type() const override;
    };

    class svg_smooth_cubic_bezier : public svg_cubic_bezier {
        void add_attributes(const std::vector<double> &values) override;

    public:
        explicit svg_smooth_cubic_bezier(bool is_relative = false) { relative = is_relative; }
    };


    class svg_circle : public svg_element {
    protected:
        void add_attributes(const std::map<std::string, std::string> &attributes) override;

        void add_attribute(const std::string &name, const std::string &value) override;

        [[nodiscard]] std::string type() const override;

    public:
        [[nodiscard]] std::string string() const override;

        void scale(double scale_x, double scale_y) override;

        void draw(svg_recorder *ctx) const override;

        void stroke(svg_recorder *ctx) const override;

        double radius{0};

        [[nodiscard]] svg_element *clone() const override;
    };


    class svg_rect : public svg_element {
    protected:
        void add_attributes(const std::map<std::string, std::string> &attributes) override;

        void add_attribute(const std::string &name, const std::string &value) override;

        [[nodiscard]] std::string type() const override;

    public:
        [[nodiscard]] std::string string() const override;

        void scale(double scale_x, double scale_y) override;

        void flip() override;

        void draw(svg_recorder *ctx) const override;

        void stroke(svg_recorder *ctx) const override;

        double radius_x{0}, radius_y{0};

        double width{0}, height{0};

        [[nodiscard]] svg_element *clone() const override;
    };


    class svg_polygon : public svg_path {
    protected:
        void add_attribute(const std::string &name, const std::string &value) override;

        [[nodiscard]] std::string type() const override;

    public:
        void set_start(const svg_point &pt) override;
    };


    class svg_use : public svg_element {
    protected:
        void add_attribute(const std::string &name, const std::string &value) override;

        [[nodiscard]] std::string type() const override;

    public:
        [[nodiscard]] std::string string() const override;

        [[nodiscard]] svg_element *clone() const override;

        void draw(svg_recorder *ctx) const override;

        void normalize(svg_point mv) override;

        void do_transform() override;

    private:
        svg_point offset{0, 0};
    };


    class svg_defs : public svg_use {
    protected:
        [[nodiscard]] std::string type() const override;

    public:
        void draw(svg_recorder *ctx) const override;
    };
} // sc

#endif //SC_SVG_ELEMENT_H
