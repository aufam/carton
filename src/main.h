#pragma once

#include <cpx/tag.h>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <chrono>

struct CompileCommand;

struct Profile {
    cpx::Tag<std::string>              id         = "toml,json:`id,skipmissing`";
    cpx::Tag<std::string>              cxx        = {"toml,json:`cxx,skipmissing`", "c++"};
    cpx::Tag<std::string>              c          = {"toml,json:`c,skipmissing`", "c"};
    cpx::Tag<bool>                     debug      = "toml,json:`debug,skipmissing`";
    cpx::Tag<bool>                     asan       = "toml,json:`asan,skipmissing`";
    cpx::Tag<bool>                     lto        = "toml,json:`lto,skipmissing`";
    cpx::Tag<int>                      opt_level  = "toml:`opt-level,skipmissing` json:`optLevel,skipmissing`";
    cpx::Tag<std::vector<std::string>> flags      = "toml,json:`flags,skipmissing,omitempty`";
    cpx::Tag<std::vector<std::string>> link_flags = "toml:`link-flags,skipmissing,omitempty`"
                                                    "json:`linkFlags,skipmissing,omitempty`";

    bool _module_support     = false;
    int  _module_cxx_version = 20;

    static Profile Release() {
        Profile t;
        t.id()        = "release";
        t.cxx()       = "c++";
        t.c()         = "cc";
        t.debug()     = false;
        t.lto()       = false;
        t.asan()      = false;
        t.opt_level() = 3;
        t.flags()     = {"-fPIC", "-Wall", "-Wextra"};
        return t;
    }

    static Profile Dev() {
        Profile t;
        t.id()        = "dev";
        t.cxx()       = "c++";
        t.c()         = "cc";
        t.debug()     = true;
        t.lto()       = false;
        t.asan()      = true;
        t.opt_level() = 0;
        t.flags()     = {"-fPIC", "-Wall", "-Wextra"};
        return t;
    }
};

struct Package {
    cpx::Tag<std::string>              name        = "toml,json:`name`";
    cpx::Tag<std::string>              version     = "toml,json:`version,skipmissing,omitempty`";
    cpx::Tag<int>                      edition     = {"toml,json:`edition,skipmissing`", 17};
    cpx::Tag<std::vector<std::string>> authors     = "toml,json:`authors,skipmissing,omitempty`";
    cpx::Tag<std::string>              description = "toml,json:`description,skipmissing,omitempty`";
    cpx::Tag<std::string>              license     = "toml,json:`license,skipmissing,omitempty`";
};

struct Dependency {
    cpx::Tag<std::string> name = "toml,json:`name,skipmissing,omitempty`";

    cpx::Tag<std::string> version = "toml,json:`version,skipmissing,omitempty,oneof=version|path|url|git`";
    cpx::Tag<std::string> path    = "toml,json:`path,skipmissing,omitempty,oneof=version|path|url|git`";
    cpx::Tag<std::string> url     = "toml,json:`url,skipmissing,omitempty,oneof=version|path|url|git`";
    cpx::Tag<std::string> git     = "toml,json:`git,skipmissing,omitempty,oneof=version|path|url|git`";
    cpx::Tag<std::string> branch  = "toml,json:`branch,skipmissing,omitempty,oneof=branch|tag`";
    cpx::Tag<std::string> tag     = "toml,json:`tag,skipmissing,omitempty,oneof=branch|tag`";
    cpx::Tag<std::string> subdir  = "toml,json:`subdir,skipmissing,omitempty`";

    cpx::Tag<std::vector<std::string>> features         = "toml,json:`features,skipmissing,omitempty`";
    cpx::Tag<bool>                     optional         = "toml,json:`optional,skipmissing,omitempty`";
    cpx::Tag<std::optional<bool>>      default_features = "toml:`default-features,skipmissing,omitempty`"
                                                          "json:`defaultFeatures,skipmissing,omitempty`";

    cpx::Tag<std::vector<std::string>> src        = "toml,json:`src,skipmissing,omitempty`";
    cpx::Tag<std::vector<std::string>> inc        = "toml,json:`inc,skipmissing,omitempty`";
    cpx::Tag<std::vector<std::string>> lib        = "toml,json:`lib,skipmissing,omitempty`";
    cpx::Tag<std::vector<std::string>> mod        = "toml,json:`mod,skipmissing,omitempty`";
    cpx::Tag<std::vector<std::string>> flags      = "toml,json:`flags,skipmissing,omitempty`";
    cpx::Tag<std::vector<std::string>> link_flags = "toml:`link-flags,skipmissing,omitempty`"
                                                    "json:`linkFlags,skipmissing,omitempty`";

    cpx::Tag<std::string> pre = "toml,json:`pre,skipmissing,omitempty`";

    std::optional<int> cpp_standard = std::nullopt;

    Dependency &operator+=(const Dependency &other);
    bool        empty() const;
};

using Lib = Dependency;

struct CompileCommand {
    cpx::Tag<std::string> file      = "json:`file`";
    cpx::Tag<std::string> directory = "json:`directory`";
    cpx::Tag<std::string> command   = "json:`command`";
    cpx::Tag<std::string> output    = "json:`output`";
    cpx::Tag<std::string> depfile   = "";
};
bool compile_multi(
    const std::string                            &name,
    const std::vector<CompileCommand>            &commands,
    std::unordered_map<std::string, std::string> &hash_history,
    bool                                          precompile = false
);

struct Project {
    using Dep = std::variant<std::string, Dependency>;

    struct Profiles {
        cpx::Tag<Profile> release = {"toml,json:`release,skipmissing`", Profile::Release()};
        cpx::Tag<Profile> dev     = {"toml,json:`dev,skipmissing`", Profile::Dev()};
    };
    cpx::Tag<Profiles> profiles = "toml,json:`profile,skipmissing`";

    cpx::Tag<std::unordered_map<std::string, Project>> packages = "toml:`packages,skipmissing,omitempty`";
    cpx::Tag<Package>                                  package  = "toml,json:`package`";

    cpx::Tag<std::unordered_map<std::string, Dep>> dependencies = "toml,json:`dependencies,skipmissing,omitempty`";
    cpx::Tag<Lib>                                  lib          = "toml,json:`lib,skipmissing`";
    cpx::Tag<std::unordered_map<std::string, std::vector<std::string>>> features = "toml,json:`features,skipmissing,omitempty`";

    cpx::Tag<std::unordered_map<std::string, std::string>> vars = "toml,json:`vars,skipmissing,omitempty`";

    cpx::Tag<std::string> cache               = "opt:`cache,env=CARTON_CACHE,skipmissing`";
    cpx::Tag<bool>        no_default_features = "opt:`no-default-features,help=Disable default features`";

    enum class LogLevel { trace, debug, info, warn, err, critical, off };
    cpx::Tag<LogLevel> log_level = {"opt:`log-level,skipmissing`", LogLevel::warn};

    struct Build {
        cpx::Tag<bool> release = "opt:`release`";
    };
    struct Run {
        cpx::Tag<bool>                     release = "opt:`release`";
        cpx::Tag<std::vector<std::string>> args    = "opt:`args,positional`";
    };
    struct Manifest {};
    cpx::Tag<Build>    _build    = "opt:`build`";
    cpx::Tag<Run>      _run      = "opt:`run`";
    cpx::Tag<Manifest> _manifest = "opt:`manifest`";

    std::unordered_map<std::string, Project> *ppackages = nullptr;
    Project                                  *pparent   = nullptr;

    struct Meta {
        Dependency                  lib;
        std::vector<std::string>    flags;
        std::vector<std::string>    link_flags;
        std::vector<std::string>    module_flags;
        std::vector<std::string>    include_flags;
        std::string                 main_o;
        std::vector<CompileCommand> compile_commands;
        std::vector<CompileCommand> precompile_commands;
    };
    std::vector<Meta>  meta;
    std::vector<Meta> *pmeta = nullptr;

    void configure(const Profile &profile, const std::vector<std::string> &features = {});
    Meta collect_meta(const Profile &profile, Dependency &dep);
    void build(
        const std::string                           &working_dir,
        const Profile                               &profile,
        bool                                         link,
        const std::vector<std::string>              &link_flags,
        bool                                         do_run,
        const std::vector<std::string>              &run_args,
        const std::chrono::system_clock::time_point &start
    );

private:
    void apply_package_placeholders();
    void resolve_remote_dep(const Profile &profile, const std::string &name, Dependency &dep);
};

Dependency &convert_dep(Project::Dep &dep);

std::string resolve_path(const std::string &cache, const std::string &path);
std::string git_clone(const std::string &cache, const std::string &git, const std::string &tag);

std::vector<std::string> expand_path(const std::string &working_dir, std::vector<std::string> &sources);
std::vector<std::string> sort_modules(const std::string &working_dir, std::vector<std::string> &sources);

void push_unique(std::vector<std::string> &vec, const std::string &value, bool front = false);
void push_unique(std::vector<std::string> &vec, const std::vector<std::string> &values, bool front = false);
