#include <cpx/cli/cli11.h>
#include <cpx/toml/toruniina_toml.h>
#include <cpx/json/yy_json.h>
#include <cpx/fmt.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <filesystem>

import carton;

namespace fs = std::filesystem;

constexpr auto toml_version = cpx::toruniina_toml::spec::v(1, 1, 0);

int main(int argc, char **argv) {
    auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("cpp++", std::move(sink)));
    spdlog::set_pattern("%^%l%$: %v");

    Cli cli = {};
    cpx::cli::cli11::parse_with_subcommands("C++ package manager", argc, argv, cli);
    spdlog::set_level(cli.log_level);

    Cache  cache = {};
    Carton ctx   = {};

    ctx.cli   = &cli;
    ctx.cache = &cache;

    try {
        auto packages = "carton-packages.toml";
        cpx::toml::toruniina_toml::parse_from_file(
            fmt::format("{}{}", std::filesystem::exists(packages) ? "" : cli.cache + "/", packages), ctx.registry, toml_version
        );
    } catch (std::exception &e) {
        spdlog::error("Failed to parse carton packages: {}", e.what());
        return 1;
    }

    try {
        cpx::toml::toruniina_toml::parse_from_file("./carton.toml", ctx, toml_version);
        if (fs::path(ctx.lib.path).is_absolute() && fs::path(ctx.lib.subdir).is_absolute())
            throw ferr("Path must be relative");
    } catch (std::exception &e) {
        spdlog::error("Failed to parse carton package: {}", e.what());
        return 1;
    }
    ctx.lib.path = (fs::current_path() / ctx.lib.path).string();

    ctx.profiles.dev._module_support =
        ctx.profiles.dev.modules == "auto" && ctx.profiles.dev.cxx.find("clang++") != std::string::npos;
    ctx.profiles.release._module_support =
        ctx.profiles.release.modules == "auto" && ctx.profiles.release.cxx.find("clang++") != std::string::npos;
    spdlog::info("provile.dev._module_support={}", ctx.profiles.dev._module_support);
    spdlog::info("provile.release._module_support={}", ctx.profiles.release._module_support);

    std::vector<CompileCommand> ccs;

    const bool  do_run       = cli.run.has_value();
    const bool  do_build     = do_run || cli.build.has_value();
    const bool  release_mode = do_run ? cli.run->release : do_build ? cli.build->release : false;
    const auto &profile      = release_mode ? ctx.profiles.release : ctx.profiles.dev;

    try {
        ctx.configure(profile);
        auto [_, m] = ctx.build(profile, ccs, do_build);
        if (do_run)
            return ctx.run(m);
    } catch (std::exception &e) {
        auto of = std::ofstream("./compile_commands.json");
        of << cpx::json::yy_json::dump(ccs, YYJSON_WRITE_PRETTY_TWO_SPACES);
        spdlog::error("Failed to build: {}", e.what());
        return 1;
    }

    auto of = std::ofstream("./compile_commands.json");
    of << cpx::json::yy_json::dump(ccs, YYJSON_WRITE_PRETTY_TWO_SPACES);

    if (cli.manifest.has_value())
        fmt::println("{}", cpx::json::yy_json::dump(ctx));

    return 0;
}
