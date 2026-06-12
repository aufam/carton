module;

#include <cpx/reflect.h>
#include <spdlog/spdlog.h>
#include <string>
#include <optional>
#include <vector>
#include "macro.h"

export module carton:cli;

export struct Cli {
    struct Build {
        bool release;
    };

    struct Run {
        bool                     release;
        std::vector<std::string> args;
    };

    using Manifest = std::tuple<>;

    std::optional<Build>      build;
    std::optional<Run>        run;
    std::optional<Manifest>   manifest;
    spdlog::level::level_enum log_level = spdlog::level::warn;
    std::string               cache;
};

// clang-format off
CPX_REFLECT(
    (Cli, ),

    ((build     , "build                                     "))
    ((run       , "run                                       "))
    ((manifest  , "manifest                                  "))
    ((log_level , "log-level , skipmissing                   "))
    ((cache     , "cache     , skipmissing , env=CARTON_CACHE"))
);

CPX_REFLECT(
    (Cli::Build, ),

    ((release , "release"))
);

CPX_REFLECT(
    (Cli::Run, ),

    ((release , "release"))
    ((args    , "args   "))
);
// clang-format on
