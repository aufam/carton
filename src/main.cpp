#include <cpx/toml/toruniina_toml.h>
#include <cpx/json/yy_json.h>
#include <cpx/cli/cli11.h>
#include <fmt/ranges.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "main.h"
#include <filesystem>

int main(int argc, char **argv) {
    auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("cpp++", std::move(sink)));
    spdlog::set_pattern("%^%l%$: %v");

    constexpr auto toml_version = cpx::toml::toruniina_toml::spec::v(1, 1, 0);

    Project ctx;
    cpx::cli::cli11::parse("C++ package manager", argc, argv, ctx);
    spdlog::set_level(spdlog::level::level_enum(ctx.log_level()));

    if (ctx.cache().empty())
        ctx.cache() = std::getenv("HOME") + std::string("/.carton");

    try {
        auto packages = "carton-packages.toml";
        cpx::toml::toruniina_toml::parse_from_file(
            fmt::format("{}{}", std::filesystem::exists(packages) ? "" : ctx.cache() + "/", packages),
            ctx.packages(),
            toml_version
        );
    } catch (std::exception &e) {
        spdlog::error("Failed to parse carton packages: {}", e.what());
        return 1;
    }

    try {
        cpx::toml::toruniina_toml::parse_from_file("./carton.toml", ctx, toml_version);
    } catch (std::exception &e) {
        spdlog::error("Failed to parse carton package: {}", e.what());
        return 1;
    }

    try {
        ctx.build_dep();
        ctx.build();
    } catch (std::exception &e) {
        spdlog::error("Failed to build: {}", e.what());
        return 1;
    }

    auto of = std::ofstream("./compile_commands.json");
    of << cpx::json::yy_json::dump(ctx.compile_commands(), YYJSON_WRITE_PRETTY_TWO_SPACES);

    return 0;
}
