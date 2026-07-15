module;

#include <string>
#include <optional>
#include <vector>

export module carton:dependency;
import :compile_command;
import cpx;

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
    std::string              visibility;


    static constexpr std::tuple __field_tags__{
        cpx::field<&Dependency::version>          = "version         , oneof=version|path|url|git",
        cpx::field<&Dependency::path>             = "path            , oneof=version|path|url|git",
        cpx::field<&Dependency::url>              = "url             , oneof=version|path|url|git",
        cpx::field<&Dependency::git>              = "git             , oneof=version|path|url|git",
        cpx::field<&Dependency::tag>              = "tag             , oneof=tag|branch|commit   ",
        cpx::field<&Dependency::branch>           = "branch          , oneof=tag|branch|commit   ",
        cpx::field<&Dependency::commit>           = "commit          , oneof=tag|branch|commit   ",
        cpx::field<&Dependency::subdir>           = "subdir          , skipmissing               ",
        cpx::field<&Dependency::features>         = "features        , skipmissing               ",
        cpx::field<&Dependency::optional>         = "optional        , skipmissing               ",
        cpx::field<&Dependency::default_features> = "default-features, skipmissing               ",
        cpx::field<&Dependency::src>              = "src             , skipmissing               ",
        cpx::field<&Dependency::inc>              = "inc             , skipmissing               ",
        cpx::field<&Dependency::lib>              = "lib             , skipmissing               ",
        cpx::field<&Dependency::mod>              = "mod             , skipmissing               ",
        cpx::field<&Dependency::flags>            = "flags           , skipmissing               ",
        cpx::field<&Dependency::link_flags>       = "link-flags      , skipmissing               ",
        cpx::field<&Dependency::pre>              = "pre             , skipmissing               ",
        cpx::field<&Dependency::visibility>       = "visibility      , skipmissing               ",
    };

    static void __from_str__(Dependency &self, std::string_view str) {
        self.version = std::string(str);
    }

    std::string                 name;
    int                         cpp_standard = 0;
    std::vector<std::string>    public_flags;
    std::vector<std::string>    mod_flags;
    std::vector<std::string>    mod_names;
    std::string                 working_dir;
    std::string                 build_dir;
    std::string                 feature_signature;
    std::vector<CompileCommand> compile_commands;
    std::vector<CompileCommand> precompile_commands;
    CompileCommand              ar_command;

    Dependency &operator+=(const Dependency &other);

    bool empty() const {
        return version.empty() && path.empty() && url.empty() && git.empty();
    }

    std::string display_name() const {
        std::string name = this->name;
        if (!version.empty()) {
            name += " v" + version;
        } else if (!tag.empty()) {
            name += " #" + tag;
        } else if (!branch.empty()) {
            name += " " + branch;
        } else if (!commit.empty()) {
            name += " " + commit;
        } else {
            name += " (" + path + ")";
        }
        return name;
    }

    std::string build_name() const {
        std::string name = this->name;
        if (!tag.empty()) {
            name += "-" + tag;
        } else if (!branch.empty()) {
            name += "-" + branch;
        } else if (!commit.empty()) {
            name += "-" + commit;
        } else if (!version.empty()) {
            name += "-v" + version;
        }
        return name;
    }
};
