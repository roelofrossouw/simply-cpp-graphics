cmake_minimum_required(VERSION 3.22)
set(SC_MODULE graphics)
project(simply-cpp-${SC_MODULE} HOMEPAGE_URL "https://github.com/roelofrossouw/simply-cpp-graphics" LANGUAGES CXX)
include(cmake/sc_bootstrap.cmake)

option(SC_GRAPHICS_BUILD_TESTS "Build simply-cpp-graphics tests" ${PROJECT_IS_TOP_LEVEL})

get_sc_version()
message(STATUS "SimplyCPP ${SC_MODULE} Version: ${SC_VERSION}")

find_or_install_package(sc-core simply-cpp-dev simply-cpp 1.8.0)
# Through pkg-config: jammy's libtinyxml2-dev has no CMake package. As with sc-redis's hiredis, the
# library is linked by path so the exported targets don't need pkg-config.
config_or_install_package(tinyxml2 libtinyxml2-dev tinyxml2)
if (SC_GRAPHICS_TINYXML2_LIBRARY AND NOT EXISTS "${SC_GRAPHICS_TINYXML2_LIBRARY}")
    message(STATUS "tinyxml2: the cached library location is gone, looking again")
    unset(SC_GRAPHICS_TINYXML2_LIBRARY CACHE)
endif ()
find_library(SC_GRAPHICS_TINYXML2_LIBRARY NAMES tinyxml2 HINTS ${tinyxml2_LIBRARY_DIRS} REQUIRED)
# sc::font_face: fontconfig finds the font, FreeType reads it. Through pkg-config too, linked by path.
config_or_install_package(fontconfig libfontconfig-dev fontconfig)
config_or_install_package(freetype2 libfreetype-dev freetype)
foreach (library fontconfig freetype)
    string(TOUPPER "SC_GRAPHICS_${library}_LIBRARY" cached)
    if (${cached} AND NOT EXISTS "${${cached}}")
        message(STATUS "${library}: the cached library location is gone, looking again")
        unset(${cached} CACHE)
    endif ()
endforeach ()
find_library(SC_GRAPHICS_FONTCONFIG_LIBRARY NAMES fontconfig HINTS ${fontconfig_LIBRARY_DIRS} REQUIRED)
find_library(SC_GRAPHICS_FREETYPE_LIBRARY NAMES freetype HINTS ${freetype2_LIBRARY_DIRS} REQUIRED)

set(SOURCE_OBJECTS include/font.h include/text.h include/typeface.h include/paint.h)
set(SOURCE_LIBRARIES)
set(SOURCE_LINK_LIBRARIES)

add_sc_object(canvas LINK_LIBRARIES sc::sc-core PUBLIC_LINK_LIBRARIES sc::sc-core)
add_sc_object(paragraph LINK_LIBRARIES sc::sc-core)
# fontconfig and FreeType stay out of the installed header.
add_sc_object(font_face
        LINK_LIBRARIES sc::sc-core PkgConfig::fontconfig PkgConfig::freetype2
        PUBLIC_LINK_LIBRARIES ${SC_GRAPHICS_FONTCONFIG_LIBRARY} ${SC_GRAPHICS_FREETYPE_LIBRARY})
add_sc_object(path LINK_LIBRARIES sc::sc-core)
# The SVG reader (on tinyxml2) stays in src/, out of the installed headers.
add_sc_object(svg
        SOURCES src/svg_element.cpp src/svg_element.h src/svg_parser.cpp src/svg_parser.h src/svg_recorder.h
        INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
        LINK_LIBRARIES sc::sc-core PkgConfig::tinyxml2
        PUBLIC_LINK_LIBRARIES ${SC_GRAPHICS_TINYXML2_LIBRARY})

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

package_sc_module(DEV_DEPENDS "simply-cpp-dev (>= 1.8.0~)" libtinyxml2-dev libfontconfig-dev libfreetype-dev)
