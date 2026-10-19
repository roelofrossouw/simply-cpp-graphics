// sc::bitmap reads a PNG's or JPEG's size from its header, and comes from files and data: URIs.
#include <bitmap.h>

#include "sc_test.h"

#include <base64.h>

#include <stdexcept>
#include <string>

namespace {
    // A 3 by 2 PNG: red, green, blue over blue, green, red.
    const std::string png = sc::base64::decode("iVBORw0KGgoAAAANSUhEUgAAAAMAAAACCAIAAAASFvFNAAAAEUlEQVR4nGP4z8DAgMAgyAAAO9gF+4ea2nQAAAAASUVORK5CYII=");

    // The start of a JPEG, 640 by 480: SOI, an APP0 segment, then SOF0.
    std::string jpeg() {
        std::string bytes{"\xFF\xD8\xFF\xE0\x00\x04\x00\x00", 8};
        bytes += std::string{"\xFF\xC0\x00\x11\x08\x01\xE0\x02\x80\x03", 10};
        return bytes;
    }
}

int main() {
    SECTION("A PNG's size and type");
    {
        const sc::bitmap picture{png};
        CHECK(picture.type() == sc::bitmap::format::png);
        CHECK_EQ(picture.size(), (sc::size{3, 2}));
        CHECK_EQ(std::string{picture.media_type()}, std::string{"image/png"});
        CHECK(sc::bitmap::recognised(png));
    }

    SECTION("A JPEG's size from its start-of-frame marker");
    {
        const sc::bitmap picture{jpeg()};
        CHECK(picture.type() == sc::bitmap::format::jpeg);
        CHECK_EQ(picture.size(), (sc::size{640, 480}));
    }

    SECTION("data: URIs, base64 and the other way round");
    {
        const sc::bitmap picture{png};
        CHECK(picture.data_uri().starts_with("data:image/png;base64,"));
        CHECK(sc::bitmap::from_data_uri(picture.data_uri()) == picture);
        CHECK_EQ(sc::bitmap::from_data_uri(picture.data_uri()).size(), (sc::size{3, 2}));
    }

    SECTION("Copies share the bytes");
    {
        const sc::bitmap picture{png};
        const sc::bitmap copy = picture;
        CHECK_EQ(&copy.bytes(), &picture.bytes());
    }

    SECTION("Mistakes");
    {
        CHECK_THROWS_AS(sc::bitmap{"GIF89a"}, std::runtime_error);
        CHECK_THROWS_AS(sc::bitmap{png.substr(0, 10)}, std::runtime_error);
        CHECK_THROWS_AS(sc::bitmap::from_data_uri("image/png;base64,abc"), std::runtime_error);
        CHECK_THROWS_AS(sc::bitmap::load("/nonexistent.png"), std::runtime_error);
        CHECK(!sc::bitmap::recognised("<svg/>"));
    }
    TEST_SUMMARY();
}
