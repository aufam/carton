cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(carton
    VERSION     0.1.0
    LANGUAGES   C CXX
)

include(cmake/CPM.cmake)

CPMAddPackage(
    NAME            cpx
    GIT_REPOSITORY  "https://github.com/aufam/cpx"
    GIT_TAG         v0.1.0
)

CPMAddPackage(
    NAME            fmt
    GIT_REPOSITORY  "https://github.com/fmtlib/fmt"
    GIT_TAG         11.2.0
    COMPONENTS      fmt
)

CPMAddPackage(
    NAME            spdlog
    GIT_REPOSITORY  "https://github.com/gabime/spdlog"
    GIT_TAG         v1.15.3
    OPTIONS         "SPDLOG_FMT_EXTERNAL ON"
    COMPONENTS      spdlog
)

CPMAddPackage(
    NAME            yyjson
    GIT_REPOSITORY  "https://github.com/ibireme/yyjson"
    GIT_TAG         0.12.0
    OPTIONS         "YYJSON_INSTALL OFF"
)

CPMAddPackage(
    NAME            toml11
    GIT_REPOSITORY  "https://github.com/ToruNiina/toml11"
    GIT_TAG         v4.4.0
    OPTIONS         "TOML11_INSTALL OFF"
)

CPMAddPackage(
    NAME            pfr
    GIT_REPOSITORY  "https://github.com/boostorg/pfr"
    GIT_TAG         boost-1.88.0
    OPTIONS         "BOOST_USE_MODULES OFF"
    COMPONENTS      Boost::pfr
)

CPMAddPackage(
    NAME            magic_enum
    GIT_REPOSITORY  "https://github.com/Neargye/magic_enum"
    GIT_TAG         v0.9.7
)

CPMAddPackage(
    NAME            CLI11
    GIT_REPOSITORY  "https://github.com/CLIUtils/CLI11"
    GIT_TAG         v2.6.1
)

# only setup the exeternal libraries
if (REQUIREMENTS)
    return()
endif()

file(GLOB_RECURSE SOURCES src/*)
add_executable(carton ${SOURCES})

target_link_libraries(carton PRIVATE
    cpx
    fmt
    spdlog
    yyjson
    toml11::toml11
    Boost::pfr
    magic_enum
    CLI11
)

target_compile_options(carton PRIVATE
    -Wall
    -Wextra
    -fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}=carton
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-redundant-move>
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)

target_link_options(carton PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    $<$<CONFIG:Release>:-static>
)
