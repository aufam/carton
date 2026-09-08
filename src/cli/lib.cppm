module;

#include <spdlog/spdlog.h>
#include <string>
#include <optional>
#include <vector>

export module carton:cli;
import :package;
import :dependency;
import cpx;

export struct Cli {
    struct Update {
        static constexpr std::tuple __field_tags__ = {};
    };

    struct Build {
        bool                     release;
        bool                     static_;
        bool                     no_default_features;
        std::vector<std::string> features;

        static constexpr std::tuple __field_tags__{
            cpx::field<&Build::release>             = "release",
            cpx::field<&Build::static_>             = "static",
            cpx::field<&Build::no_default_features> = "no-default-features",
            cpx::field<&Build::features>            = "features",
        };
    };

    struct Run {
        bool                     release;
        bool                     static_;
        std::string              bin;
        std::vector<std::string> args;

        static constexpr std::tuple __field_tags__{
            cpx::field<&Run::release> = "release",
            cpx::field<&Run::static_> = "static",
            cpx::field<&Run::bin>     = "bin,skipmissing",
            cpx::field<&Run::args>    = "args,positional",
        };
    };

    std::string               cache;
    spdlog::level::level_enum log_level = spdlog::level::warn;
    bool                      release;
    bool                      static_;
    bool                      no_default_features;
    std::vector<std::string>  features;

    std::optional<Update>     update;
    std::optional<Package>    init;
    std::optional<Dependency> add;

    std::optional<Build> build;
    std::optional<Run>   run;

    static constexpr std::tuple __field_tags__ = {
        cpx::field<&Cli::cache>               = "cache              , skipmissing , env=CARTON_CACHE",
        cpx::field<&Cli::log_level>           = "log-level          , skipmissing                   ",
        cpx::field<&Cli::build>               = "build                                              ",
        cpx::field<&Cli::run>                 = "run                                                ",
        cpx::field<&Cli::release>             = "release                                            ",
        cpx::field<&Cli::static_>             = "static                                             ",
        cpx::field<&Cli::no_default_features> = "no-default-features                                ",
        cpx::field<&Cli::features>            = "features                                           ",
        cpx::field<&Cli::init>                = "init                                               ",
        cpx::field<&Cli::add>                 = "add                                                ",
        cpx::field<&Cli::update>              = "update                                             ",
    };
};
