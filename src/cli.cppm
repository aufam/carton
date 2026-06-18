module;

#include <cpx/reflect.h>
#include <spdlog/spdlog.h>
#include <string>
#include <optional>
#include <vector>
#include "macro.h"

export module carton:cli;

export struct Cli {
    struct Manifest {
        bool                     release;
        bool                     no_default_features;
        std::vector<std::string> features;
    };

    struct Build {
        bool                     release;
        bool                     no_default_features;
        std::vector<std::string> features;
    };

    struct Run {
        bool                     release;
        bool                     no_default_features;
        std::vector<std::string> features;
        std::vector<std::string> args;
    };

    std::string               cache;
    spdlog::level::level_enum log_level = spdlog::level::warn;
    std::optional<Manifest>   manifest;
    std::optional<Build>      build;
    std::optional<Run>        run;
    bool                      release;
    bool                      no_default_features;
    std::vector<std::string>  features;
};

// clang-format off
CPX_REFLECT(
    (Cli, ),

    ((cache              , "cache              , skipmissing , env=CARTON_CACHE"))
    ((log_level          , "log-level          , skipmissing                   "))
    ((manifest           , "manifest                                           "))
    ((build              , "build                                              "))
    ((run                , "run                                                "))
    ((release            , "release                                            "))
    ((no_default_features, "no-default-features                                "))
    ((features           , "features                                           "))
);

CPX_REFLECT(
    (Cli::Manifest, ),

    ((release            , "release                                            "))
    ((no_default_features, "no-default-features                                "))
    ((features           , "features                                           "))
);

CPX_REFLECT(
    (Cli::Build, ),

    ((release            , "release                                            "))
    ((no_default_features, "no-default-features                                "))
    ((features           , "features                                           "))
);

CPX_REFLECT(
    (Cli::Run, ),

    ((release            , "release                                            "))
    ((no_default_features, "no-default-features                                "))
    ((features           , "features                                           "))
    ((args               , "args                                               "))
);
// clang-format on
