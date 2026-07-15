module;

#include <spdlog/spdlog.h>
#include <string>
#include <optional>
#include <vector>

export module carton:cli;
import cpx;

export struct Cli {
    struct Manifest {
        bool                     release;
        bool                     no_default_features;
        std::vector<std::string> features;

        static constexpr std::tuple __field_tags__{
            cpx::field<&Manifest::release>             = "release",
            cpx::field<&Manifest::no_default_features> = "no-default-features",
            cpx::field<&Manifest::features>            = "features",
        };
    };

    struct Build {
        bool                     release;
        bool                     no_default_features;
        std::vector<std::string> features;

        static constexpr std::tuple __field_tags__{
            cpx::field<&Build::release>             = "release",
            cpx::field<&Build::no_default_features> = "no-default-features",
            cpx::field<&Build::features>            = "features",
        };
    };

    struct Run {
        bool                     release;
        bool                     no_default_features;
        std::vector<std::string> features;
        std::vector<std::string> args;

        static constexpr std::tuple __field_tags__{
            cpx::field<&Run::release>             = "release",
            cpx::field<&Run::no_default_features> = "no-default-features",
            cpx::field<&Run::features>            = "features",
            cpx::field<&Run::args>                = "args,positional",
        };
    };

    std::string               cache;
    spdlog::level::level_enum log_level = spdlog::level::warn;
    std::optional<Manifest>   manifest;
    std::optional<Build>      build;
    std::optional<Run>        run;
    bool                      release;
    bool                      no_default_features;
    std::vector<std::string>  features;

    static constexpr std::tuple __field_tags__{
        cpx::field<&Cli::cache>               = "cache              , skipmissing , env=CARTON_CACHE",
        cpx::field<&Cli::log_level>           = "log-level          , skipmissing                   ",
        cpx::field<&Cli::manifest>            = "manifest                                           ",
        cpx::field<&Cli::build>               = "build                                              ",
        cpx::field<&Cli::run>                 = "run                                                ",
        cpx::field<&Cli::release>             = "release                                            ",
        cpx::field<&Cli::no_default_features> = "no-default-features                                ",
        cpx::field<&Cli::features>            = "features                                           ",
    };
};
