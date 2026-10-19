#include "bitmap.h"

#include <base64.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>

namespace sc {
    namespace {
        constexpr std::string_view png_signature{"\x89PNG\r\n\x1a\n", 8};

        unsigned byte_at(const std::string_view bytes, const std::size_t at) { return static_cast<unsigned char>(bytes[at]); }

        unsigned big_endian(const std::string_view bytes, const std::size_t at, const int count) {
            unsigned value = 0;
            for (int i = 0; i < count; ++i) value = value << 8 | byte_at(bytes, at + i);
            return value;
        }

        // The width and height in the IHDR chunk, which comes first.
        std::optional<sc::size> png_size(const std::string_view bytes) {
            if (bytes.size() < 24 || bytes.substr(12, 4) != "IHDR") return std::nullopt;
            return sc::size{static_cast<double>(big_endian(bytes, 16, 4)), static_cast<double>(big_endian(bytes, 20, 4))};
        }

        // The size in the first start-of-frame marker (SOF0 to SOF15, not DHT, JPG or DAC), past
        // the segments before it.
        std::optional<sc::size> jpeg_size(const std::string_view bytes) {
            std::size_t at = 2;
            while (at + 4 <= bytes.size()) {
                if (byte_at(bytes, at) != 0xFF) return std::nullopt;
                const unsigned marker = byte_at(bytes, at + 1);
                if (marker == 0xFF) {
                    ++at; // padding
                    continue;
                }
                if (marker == 0xD8 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {
                    at += 2; // no length
                    continue;
                }
                const unsigned length = big_endian(bytes, at + 2, 2);
                if (marker >= 0xC0 && marker <= 0xCF && marker != 0xC4 && marker != 0xC8 && marker != 0xCC) {
                    if (at + 9 > bytes.size()) return std::nullopt;
                    return sc::size{static_cast<double>(big_endian(bytes, at + 7, 2)), static_cast<double>(big_endian(bytes, at + 5, 2))};
                }
                at += 2 + length;
            }
            return std::nullopt;
        }

        std::string percent_decoded(const std::string_view text) {
            std::string out;
            for (std::size_t i = 0; i < text.size(); ++i) {
                if (text[i] == '%' && i + 2 < text.size() && std::isxdigit(static_cast<unsigned char>(text[i + 1])) &&
                    std::isxdigit(static_cast<unsigned char>(text[i + 2]))) {
                    out += static_cast<char>(std::stoi(std::string{text.substr(i + 1, 2)}, nullptr, 16));
                    i += 2;
                } else {
                    out += text[i];
                }
            }
            return out;
        }
    }

    bool bitmap::recognised(const std::string_view bytes) {
        return bytes.starts_with(png_signature) || bytes.starts_with("\xFF\xD8\xFF");
    }

    bitmap::bitmap(std::string bytes) {
        std::optional<sc::size> found;
        if (bytes.starts_with(png_signature)) {
            type_ = format::png;
            found = png_size(bytes);
        } else if (bytes.starts_with("\xFF\xD8\xFF")) {
            type_ = format::jpeg;
            found = jpeg_size(bytes);
        } else {
            throw std::runtime_error("sc::bitmap: not a PNG or JPEG image");
        }
        if (!found || found->width() <= 0 || found->height() <= 0) throw std::runtime_error("sc::bitmap: can't read the image's size");
        size_ = *found;
        bytes_ = std::make_shared<const std::string>(std::move(bytes));
    }

    bitmap bitmap::load(const std::filesystem::path &file) {
        std::ifstream in{file, std::ios::binary};
        if (!in) throw std::runtime_error("sc::bitmap: can't read " + file.string());
        return bitmap{std::string{std::istreambuf_iterator<char>{in}, {}}};
    }

    bitmap bitmap::from_data_uri(const std::string_view uri) {
        if (!uri.starts_with("data:")) throw std::runtime_error("sc::bitmap: not a data: URI");
        const auto comma = uri.find(',');
        if (comma == std::string_view::npos) throw std::runtime_error("sc::bitmap: a data: URI without data");
        const auto header = uri.substr(5, comma - 5);
        const auto data = uri.substr(comma + 1);
        const bool base64 = header.ends_with(";base64");
        return bitmap{base64 ? sc::base64::decode(percent_decoded(data)) : percent_decoded(data)};
    }

    std::string_view bitmap::media_type() const { return type_ == format::png ? "image/png" : "image/jpeg"; }

    std::string bitmap::data_uri() const { return "data:" + std::string{media_type()} + ";base64," + sc::base64::encode(*bytes_); }
}
