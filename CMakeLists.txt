cmake_minimum_required(VERSION 3.22)
set(SC_MODULE graphics)
project(simply-cpp-${SC_MODULE} HOMEPAGE_URL "https://github.com/roelofrossouw/simply-cpp-graphics" LANGUAGES CXX)
include(cmake/sc_bootstrap.cmake)

option(SC_GRAPHICS_BUILD_TESTS "Build simply-cpp-graphics tests" ${PROJECT_IS_TOP_LEVEL})

get_sc_version()
message(STATUS "SimplyCPP ${SC_MODULE} Version: ${SC_VERSION}")

find_or_install_package(sc-core simply-cpp-dev simply-cpp 1.20.0)
# SVG is read as XML by sc-web (libxml2).
find_or_install_package(sc-web simply-cpp-web-dev simply-cpp-web 1.4.7)
# sc::font_face: fontconfig finds the font, FreeType reads it. Through pkg-config too, linked by path.
config_or_install_package(fontconfig libfontconfig-dev fontconfig)
config_or_install_package(freetype2 libfreetype-dev freetype)
# HarfBuzz shapes emoji sequences (flags, skin tones, joined ones) into their glyphs.
config_or_install_package(harfbuzz libharfbuzz-dev harfbuzz)
foreach (library fontconfig freetype harfbuzz)
    string(TOUPPER "SC_GRAPHICS_${library}_LIBRARY" cached)
    if (${cached} AND NOT EXISTS "${${cached}}")
        message(STATUS "${library}: the cached library location is gone, looking again")
        unset(${cached} CACHE)
    endif ()
endforeach ()
find_library(SC_GRAPHICS_FONTCONFIG_LIBRARY NAMES fontconfig HINTS ${fontconfig_LIBRARY_DIRS} REQUIRED)
find_library(SC_GRAPHICS_FREETYPE_LIBRARY NAMES freetype HINTS ${freetype2_LIBRARY_DIRS} REQUIRED)
find_library(SC_GRAPHICS_HARFBUZZ_LIBRARY NAMES harfbuzz HINTS ${harfbuzz_LIBRARY_DIRS} REQUIRED)

# These paths go into the exported targets, so not Homebrew's versioned Cellar folder (removed on
# every upgrade or reinstall, breaking whatever links sc-graphics) but its opt link, which follows.
foreach (library FONTCONFIG FREETYPE HARFBUZZ)
    set(found "${SC_GRAPHICS_${library}_LIBRARY}")
    if (found MATCHES "^(.*)/Cellar/([^/]+)/[^/]+/(.*)$" AND EXISTS "${CMAKE_MATCH_1}/opt/${CMAKE_MATCH_2}/${CMAKE_MATCH_3}")
        set(SC_GRAPHICS_${library}_LIBRARY "${CMAKE_MATCH_1}/opt/${CMAKE_MATCH_2}/${CMAKE_MATCH_3}" CACHE FILEPATH "" FORCE)
    endif ()
endforeach ()

set(SOURCE_OBJECTS include/font.h include/text.h include/typeface.h include/page.h)
set(SOURCE_LIBRARIES)
set(SOURCE_LINK_LIBRARIES)

add_sc_object(paint LINK_LIBRARIES sc::sc-core)
add_sc_object(bitmap LINK_LIBRARIES sc::sc-core)
add_sc_object(standard_font LINK_LIBRARIES sc::sc-core)
add_sc_object(canvas INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src LINK_LIBRARIES sc::sc-core PUBLIC_LINK_LIBRARIES sc::sc-core)
add_sc_object(paragraph LINK_LIBRARIES sc::sc-core)
# fontconfig, FreeType and HarfBuzz stay out of the installed headers.
add_sc_object(font_face
        SOURCES src/glyphs.h
        INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
        LINK_LIBRARIES sc::sc-core PkgConfig::fontconfig PkgConfig::freetype2 PkgConfig::harfbuzz
        PUBLIC_LINK_LIBRARIES ${SC_GRAPHICS_FONTCONFIG_LIBRARY} ${SC_GRAPHICS_FREETYPE_LIBRARY} ${SC_GRAPHICS_HARFBUZZ_LIBRARY})
# Drawings, and reading and writing them as SVG (through sc-web, out of the installed headers).
add_sc_object(drawing
        SOURCES src/drawing_svg.cpp
        LINK_LIBRARIES sc::sc-core sc::sc-web
        PUBLIC_LINK_LIBRARIES sc::sc-web)

add_sc_libraries(DESCRIPTION "simply-cpp-graphics")

install_sc_module(PATH_VARS CMAKE_INSTALL_INCLUDEDIR)
install(FILES cmake/SimplyCppFunctions.cmake cmake/sc_bootstrap.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sc-graphics
        COMPONENT development)

if (SC_GRAPHICS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests sc-graphics-test)
endif ()

add_sc_example(sc-graphics-demo)

# sc::canvas has a primitive more since 5.3.0 (paint_bitmap), so its vtable changed: engines (and
# sc-ai, which draws) built against an earlier one would call the wrong function, so they're
# upgraded with it.
# An emoji font, so emoji have a face to fall back on (a colour one).
package_sc_module(DEPENDS "simply-cpp-web (>= 1.4.7~)" fonts-noto-color-emoji
        DEV_DEPENDS "simply-cpp-dev (>= 1.20.0~)" "simply-cpp-web-dev (>= 1.4.7~)" libfontconfig-dev libfreetype-dev libharfbuzz-dev
        BREAKS "simply-cpp-image (<< 5.1.0~)" "simply-cpp-pdf (<< 3.1.0~)" "simply-cpp-mupdf (<< 4.1.0~)" "simply-cpp-ai (<< 2.0.2~)"
        "simply-cpp-litehtml (<< 1.0.0~)"
        # page_size's values are A3, A4 and A5 since 6.0.0: headers naming a4 don't compile against it.
        DEV_BREAKS "simply-cpp-pdf-dev (<< 3.1.2~)" "simply-cpp-mupdf-dev (<< 4.1.2~)" "simply-cpp-html2pdf-dev (<< 1.0.2~)")
