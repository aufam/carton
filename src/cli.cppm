module;

#include <cpx/reflect.h>
#include <spdlog/spdlog.h>
#include <string>
#include <optional>
#include <vector>

export module carton:cli;

export struct Cli {
    struct Build {
        bool release;
    };

    struct Run {
        bool                     release;
        std::vector<std::string> args;
    };

    struct Manifest {};

    std::optional<Build>      build;
    std::optional<Run>        run;
    std::optional<Manifest>   manifest;
    spdlog::level::level_enum log_level = spdlog::level::warn;
    std::string               cache     = "~/.carton";
};

template <>
struct cpx::Reflect<Cli> : Fields<Reflect<Cli>, &Cli::build, &Cli::run, &Cli::manifest, &Cli::log_level, &Cli::cache> {
    static constexpr TagInfo build     = "build";
    static constexpr TagInfo run       = "run";
    static constexpr TagInfo manifest  = "manifest";
    static constexpr TagInfo log_level = "log-level,skipmissing";
    static constexpr TagInfo cache     = "cache,skipmissing,env=CARTON_CACHE";

    static constexpr tags_type tags() {
        return std::tie(build, run, manifest, log_level, cache);
    }
};

template <>
struct cpx::Reflect<Cli::Build> : Fields<Reflect<Cli::Build>, &Cli::Build::release> {
    static constexpr TagInfo release = "release";

    static constexpr tags_type tags() {
        return std::tie(release);
    }
};

template <>
struct cpx::Reflect<Cli::Run> : Fields<Reflect<Cli::Run>, &Cli::Run::release, &Cli::Run::args> {
    static constexpr TagInfo release = "release";
    static constexpr TagInfo args    = "args";

    static constexpr tags_type tags() {
        return std::tie(release, args);
    }
};
