#include <cpx/toml/toruniina_toml.h>
#include <cpx/json/yy_json.h>
#include <cpx/cli/cli11.h>
#include <fmt/ranges.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "main.h"

int main(int argc, char **argv) {
    auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("cpp++", std::move(sink)));
    spdlog::set_pattern("%^%l%$: %v");

    constexpr auto toml_version = cpx::toml::toruniina_toml::spec::v(1, 1, 0);

    Project ctx;
    cpx::cli::cli11::parse("c++ plusplus", argc, argv, ctx);
    spdlog::set_level(spdlog::level::level_enum(ctx.log_level()));

    cpx::toml::toruniina_toml::parse_from_file("./carton-packages.toml", ctx.packages(), toml_version);
    cpx::toml::toruniina_toml::parse_from_file("./carton.toml", ctx, toml_version);

    if (ctx.cache().empty())
        ctx.cache() = std::getenv("HOME") + std::string("/.carton");

    try {
        ctx.build();
    } catch (std::exception &e) {
        spdlog::error("Failed to build: {}", e.what());
        exit(1);
    }

    std::cout << cpx::json::yy_json::dump(ctx, YYJSON_WRITE_PRETTY_TWO_SPACES) << '\n';
    return 0;
}
