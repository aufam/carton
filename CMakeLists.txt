cmake_minimum_required(VERSION 3.28 FATAL_ERROR)

project(carton
    VERSION     0.1.0
    LANGUAGES   C CXX
)

include(cmake/CPM.cmake)

CPMAddPackage(
    NAME            fmt
    GIT_REPOSITORY  "https://github.com/fmtlib/fmt"
    GIT_TAG         12.1.0
    OPTIONS         "FMT_MODULE ON"
)
target_compile_definitions(fmt PRIVATE FMT_ATTACH_TO_GLOBAL_MODULE=1)

CPMAddPackage(
    NAME            spdlog
    GIT_REPOSITORY  "https://github.com/gabime/spdlog"
    GIT_TAG         v1.15.3
    OPTIONS         "SPDLOG_FMT_EXTERNAL ON"
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

CPMAddPackage(
    NAME            reproc
    GIT_REPOSITORY  "https://github.com/daandemeyer/reproc"
    GIT_TAG         v14.2.7
    OPTIONS         "REPROC++ ON" "REPROC_INSTALL OFF" "REPROC_INSTALL_PKGCONFIG OFF"
)

CPMAddPackage(
    NAME            preprocessor
    GIT_REPOSITORY  "https://github.com/boostorg/preprocessor"
    GIT_TAG         boost-1.88.0
)

CPMAddPackage(
    NAME            cpx
    GIT_REPOSITORY  "https://github.com/aufam/cpx"
    GIT_TAG         v0.1.0
    OPTIONS         "CPX_MODULES ON"
)

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

add_executable(carton)
target_sources(carton
    PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES FILES
        ${MODULE_FILES}
    PRIVATE
        ${MODULE_IMPL_FILES}
)

target_link_libraries(carton PRIVATE
    cpx_mod
    fmt
    spdlog
    yyjson
    toml11::toml11
    magic_enum
    CLI11
    xxhash
    reproc++
    Boost::preprocessor
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
