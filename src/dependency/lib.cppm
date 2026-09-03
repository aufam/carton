module;

#include <string>
#include <optional>
#include <vector>

export module carton:dependency;
import :compile_command;
import cpx;
import cpx.cli;

export struct Dependency {
    std::string version;
    std::string path;
    std::string url;
    std::string git;
    std::string tag;
    std::string branch;
    std::string commit;
    std::string subdir;

    std::vector<std::string> features;
    bool                     optional = false;
    std::optional<bool>      default_features;

    std::vector<std::string> src;
    std::vector<std::string> inc;
    std::vector<std::string> lib;
    std::vector<std::string> mod;
    std::vector<std::string> flags;
    std::vector<std::string> link_flags;
    std::string              pre;


    static constexpr std::tuple __field_tags__{
        cpx::field<&Dependency::version>          = "version         , oneof=version|path|url|git",
        cpx::field<&Dependency::path>             = "path            , oneof=version|path|url|git",
        cpx::field<&Dependency::url>              = "url             , oneof=version|path|url|git",
        cpx::field<&Dependency::git>              = "git             , oneof=version|path|url|git",
        cpx::field<&Dependency::tag>              = "tag             , oneof=tag|branch|commit   ",
        cpx::field<&Dependency::branch>           = "branch          , oneof=tag|branch|commit   ",
        cpx::field<&Dependency::commit>           = "commit          , oneof=tag|branch|commit   ",
        cpx::field<&Dependency::subdir>           = "subdir          , skipmissing , omitempty   ",
        cpx::field<&Dependency::features>         = "features        , skipmissing , omitempty   ",
        cpx::field<&Dependency::optional>         = "optional        , skipmissing , omitempty   ",
        cpx::field<&Dependency::default_features> = "default-features, skipmissing , omitempty   ",
        cpx::field<&Dependency::src>              = "src             , skipmissing , omitempty   ",
        cpx::field<&Dependency::inc>              = "inc             , skipmissing , omitempty   ",
        cpx::field<&Dependency::lib>              = "lib             , skipmissing , omitempty   ",
        cpx::field<&Dependency::mod>              = "mod             , skipmissing , omitempty   ",
        cpx::field<&Dependency::flags>            = "flags           , skipmissing , omitempty   ",
        cpx::field<&Dependency::link_flags>       = "link-flags      , skipmissing , omitempty   ",
        cpx::field<&Dependency::pre>              = "pre             , skipmissing , omitempty   ",
    };

    std::string name; // only for cli

    static void __from_str__(Dependency &self, std::string_view str) {
        self.version = std::string(str);
    }
};

template <>
struct cpx::cli::Reflect<Dependency> {
    static constexpr std::tuple field_tags = {
        cpx::field<&Dependency::name>             = "name            , positional                ",
        cpx::field<&Dependency::version>          = "version,short=v , oneof=version|path|url|git,skipmissing",
        cpx::field<&Dependency::path>             = "path            , oneof=version|path|url|git,skipmissing",
        cpx::field<&Dependency::url>              = "url             , oneof=version|path|url|git,skipmissing",
        cpx::field<&Dependency::git>              = "git             , oneof=version|path|url|git,skipmissing",
        cpx::field<&Dependency::tag>              = "tag             , oneof=tag|branch|commit,skipmissing   ",
        cpx::field<&Dependency::branch>           = "branch          , oneof=tag|branch|commit,skipmissing   ",
        cpx::field<&Dependency::commit>           = "commit          , oneof=tag|branch|commit,skipmissing   ",
        cpx::field<&Dependency::subdir>           = "subdir          , skipmissing , omitempty   ",
        cpx::field<&Dependency::features>         = "features        , skipmissing , omitempty   ",
        cpx::field<&Dependency::optional>         = "optional        , skipmissing , omitempty   ",
        cpx::field<&Dependency::default_features> = "default-features, skipmissing , omitempty   ",
    };
};
