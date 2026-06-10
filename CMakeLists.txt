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
    OPTIONS         "TOML11_INSTALL OFF" "TOML11_PRECOMPILE ON"
)
target_compile_options(toml11 PUBLIC "-Wno-deprecated-literal-operator")

CPMAddPackage(
    NAME            magic_enum
    GIT_REPOSITORY  "https://github.com/Neargye/magic_enum"
    GIT_TAG         v0.9.7
)

CPMAddPackage(
    NAME            CLI11
    GIT_REPOSITORY  "https://github.com/CLIUtils/CLI11"
    GIT_TAG         v2.6.1
    OPTIONS         "CLI11_PRECOMPILED ON"
)

CPMAddPackage(
    NAME            xxhash
    GIT_REPOSITORY  "https://github.com/Cyan4973/xxHash"
    GIT_TAG         v0.8.3
    DOWNLOAD_ONLY   YES
)
add_library(xxhash STATIC ${xxhash_SOURCE_DIR}/xxhash.c)
target_include_directories(xxhash PUBLIC ${xxhash_SOURCE_DIR})

# only setup the exeternal libraries
if (REQUIREMENTS)
    return()
endif()


file(GLOB MODULE_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cppm"
)

file(GLOB MODULE_IMPL_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(carton)
target_sources(carton
    PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
        ${MODULE_FILES}
    PRIVATE
        ${MODULE_IMPL_FILES}
)

target_link_libraries(carton PRIVATE
    cpx
    fmt
    spdlog
    yyjson
    toml11::toml11
    magic_enum
    CLI11
    xxhash
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
