#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <reproc++/run.hpp>

import carton;
import fmt;
import cpx;
import cpx.fmt;
import cpx.yy_json;
import cpx.toruniina_toml;
import cpx.cli11;
import std.fs;

constexpr auto toml_version = cpx::toruniina_toml::spec::v(1, 1, 0);

int main(int argc, char **argv) {
    // spdlog
    auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("carton", std::move(sink)));
    spdlog::set_pattern("%^%l%$: %v");

    // cli
    Cli cli   = {};
    cli.cache = std::getenv("HOME") + std::string("/.carton");

    cpx::cli11::parse("C++ package manager and build system", argc, argv, cli);
    spdlog::set_level(cli.log_level);

    // context
    Cache  cache = {};
    Carton ctx   = {};

    ctx.cli   = &cli;
    ctx.cache = &cache;

    // parse registry
    const auto registry_path = [&]() {
        fs::path ret = ".carton/registry.toml";
        if (!fs::exists(ret))
            ret = fs::path(cli.cache) / "registry.toml";
        return ret;
    }();

    try {
        cpx::toruniina_toml::parse_from_file(registry_path.string(), ctx.registry, toml_version);
    } catch (std::exception &e) {
        spdlog::error("Failed to parse carton packages: {}", e.what());
        return 1;
    }

    // parse config
    const auto config_path = [&]() {
        fs::path ret = ".carton/config.toml";
        if (!fs::exists(ret))
            ret = fs::path(cli.cache) / "config.toml";
        return ret;
    }();

    if (fs::exists(config_path))
        try {
            auto cfg     = cpx::toruniina_toml::parse_from_file<Config>(config_path.string(), toml_version);
            ctx.profiles = cfg.profiles;
        } catch (std::exception &e) {
            spdlog::error("Failed to parse carton config: {}", e.what());
            return 1;
        }

    // parse context
    try {
        cpx::toruniina_toml::parse_from_file("./carton.toml", ctx, toml_version);
    } catch (std::exception &e) {
        spdlog::error("Failed to parse carton package: {}", e.what());
        return 1;
    }

    // validate
    if (fs::path(ctx.lib.path).is_absolute() || fs::path(ctx.lib.subdir).is_absolute()) {
        spdlog::error("<carton>.lib.path must be relative");
        return 1;
    }
    ctx.lib.path = (fs::current_path() / ctx.lib.path).string();
    ctx.profiles.check_module_support();

    // execute
    const bool  manifest = cli.manifest.has_value();
    const bool  run      = cli.run.has_value();
    const bool  build    = cli.build.has_value();
    const bool  release  = run ? cli.run->release : build ? cli.build->release : manifest ? cli.manifest->release : cli.release;
    const auto &profile  = release ? ctx.profiles.release : ctx.profiles.dev;
    const auto &features = run        ? cli.run->features
                           : build    ? cli.build->features
                           : manifest ? cli.manifest->features
                                      : cli.features;

    ctx.no_default_features = run        ? cli.run->no_default_features
                              : build    ? cli.build->no_default_features
                              : manifest ? cli.manifest->no_default_features
                                         : cli.no_default_features;

    std::vector<CompileCommand> ccs;
    try {
        ctx.configure(profile, features);
        auto [_, m] = ctx.build(profile, ccs, build || run);
        if (run)
            return ctx.run(m);
    } catch (std::exception &e) {
        auto of = std::ofstream("./compile_commands.json");
        of << cpx::yy_json::dump(ccs, cpx::yy_json::write_flag::pretty_two_spaces);
        spdlog::error("Failed to build: {}", e.what());
        return 1;
    }

    auto of = std::ofstream("./compile_commands.json");
    of << cpx::yy_json::dump(ccs, cpx::yy_json::write_flag::pretty_two_spaces);

    if (manifest)
        fmt::println("{}", cpx::yy_json::dump(ctx));

    return 0;
}
