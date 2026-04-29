#include <cpx/toml/toruniina_toml.h>
#include <cpx/json/yy_json.h>
#include <cpx/cli/cli11.h>
#include <fmt/ranges.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <filesystem>
#include "main.h"

#define f(...)    fmt::format(__VA_ARGS__)
#define ferr(...) std::runtime_error(fmt::format(__VA_ARGS__))
namespace fs = std::filesystem;


int main(int argc, char **argv) {
    auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("cpp++", std::move(sink)));
    spdlog::set_pattern("%^%l%$: %v");

    constexpr auto toml_version = cpx::toml::toruniina_toml::spec::v(1, 1, 0);

    auto start = std::chrono::system_clock::now();

    Project ctx;
    auto    subcommands = cpx::cli::cli11::parse_with_subcommands("C++ package manager", argc, argv, ctx);
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
        if (fs::path(ctx.lib().path()).is_absolute() && fs::path(ctx.lib().subdir()).is_absolute())
            throw ferr("Path must be relative");
    } catch (std::exception &e) {
        spdlog::error("Failed to parse carton package: {}", e.what());
        return 1;
    }
    ctx.lib().path() = (fs::current_path() / ctx.lib().path()).string();

    ctx.profiles().dev()._module_cxx_version     = std::max(ctx.package().edition(), 20);
    ctx.profiles().release()._module_cxx_version = std::max(ctx.package().edition(), 20);
    ctx.profiles().dev()._module_support         = ctx.profiles().dev().cxx().find("clang++") != std::string::npos;
    ctx.profiles().release()._module_support     = ctx.profiles().release().cxx().find("clang++") != std::string::npos;

    std::vector<CompileCommand> ccs;
    const std::string          &subcommand = subcommands.empty() ? "" : subcommands.front();
    try {
        const auto  do_run       = subcommand == "run";
        const auto  do_build     = do_run || (subcommand == "build");
        const auto  release_mode = ctx._build().release() || ctx._run().release();
        const auto &profile      = release_mode ? ctx.profiles().release() : ctx.profiles().dev();
        ctx.configure(profile);

        std::unordered_map<std::string, std::string> hash;
        bool                                         relink = false;
        for (const auto &m : ctx.meta) {
            std::string name = m.lib.name();
            if (!m.lib.version().empty()) {
                name += " v" + m.lib.version();
            } else if (!m.lib.tag().empty()) {
                name += " #" + m.lib.tag();
            } else if (!m.lib.branch().empty()) {
                name += " " + m.lib.branch();
            } else {
                name += " (" + m.lib.path() + ")";
            }
            if (do_build)
                relink |= compile_multi(name, m.compile_commands, hash);
            ccs.insert(ccs.end(), m.compile_commands.begin(), m.compile_commands.end());
        }

        auto        m    = ctx.collect_meta(profile, ctx.lib());
        std::string name = m.lib.name();
        if (!m.lib.version().empty()) {
            name += " v" + m.lib.version();
        } else if (!m.lib.tag().empty()) {
            name += " #" + m.lib.tag();
        } else if (!m.lib.branch().empty()) {
            name += " " + m.lib.branch();
        } else {
            name += " (" + m.lib.path() + ")";
        }
        if (do_build)
            relink |= compile_multi(name, m.compile_commands, hash);
        ccs.insert(ccs.end(), m.compile_commands.begin(), m.compile_commands.end());

        if (do_build && !m.compile_commands.empty())
            ctx.build(m.compile_commands.front().directory(), profile, relink, m.link_flags, do_run, ctx._run().args(), start);
    } catch (std::exception &e) {
        spdlog::error("Failed to build: {}", e.what());
        return 1;
    }

    auto of = std::ofstream("./compile_commands.json");
    of << cpx::json::yy_json::dump(ccs, YYJSON_WRITE_PRETTY_TWO_SPACES);

    if (subcommand == "manifest") {
        fmt::println("{}", cpx::json::yy_json::dump(ctx));
    }

    return 0;
}
