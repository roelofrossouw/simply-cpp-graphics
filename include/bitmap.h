#pragma once

#include <size.h>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace sc {
    /// A raster image as its encoded file, PNG or JPEG, to draw on any canvas: PDFs embed it as it
    /// is, an sc::image decodes it, an sc::drawing keeps it (and SVG holds it as a data: URI).
    /// Copies share the bytes.
    ///
    ///     const auto logo = sc::bitmap::load("logo.png");
    ///     canvas.draw(logo, sc::rect{10, 10, 120, 40});
    class bitmap {
    public:
        enum class format { png, jpeg };

        /// A PNG or JPEG file's bytes; throws std::runtime_error for anything else, or when its
        /// size can't be read from it.
        explicit bitmap(std::string bytes);

        /// Reads a PNG or JPEG file; throws std::runtime_error when it can't.
        [[nodiscard]] static bitmap load(const std::filesystem::path &file);

        /// A data: URI's image ("data:image/png;base64,..."), base64 or percent-encoded; throws
        /// std::runtime_error when it isn't one, or holds no PNG or JPEG.
        [[nodiscard]] static bitmap from_data_uri(std::string_view uri);

        [[nodiscard]] const std::string &bytes() const { return *bytes_; }

        [[nodiscard]] format type() const { return type_; }

        /// "image/png" or "image/jpeg".
        [[nodiscard]] std::string_view media_type() const;

        /// "data:image/png;base64,...".
        [[nodiscard]] std::string data_uri() const;

        /// In pixels.
        [[nodiscard]] sc::size size() const { return size_; }

        /// Whether a file's bytes are a PNG or a JPEG, by their start.
        [[nodiscard]] static bool recognised(std::string_view bytes);

        bool operator==(const bitmap &other) const { return bytes_ == other.bytes_ || *bytes_ == *other.bytes_; }

    private:
        std::shared_ptr<const std::string> bytes_;
        format type_;
        sc::size size_;
    };
}
