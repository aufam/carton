#pragma once

#include <cpx/tag.h>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

struct CompileCommand;

struct Target {
    cpx::Tag<std::string> id  = "toml,json:`id`";
    cpx::Tag<std::string> cpp = {"toml,json:`c++,skipmissing`", "c++"};
    cpx::Tag<std::string> c   = {"toml,json:`c,skipmissing`", "c"};

    static Target Release() {
        Target t;
        t.id()  = "release";
        t.cpp() = "c++ -O3 -DNDEBUG -fPIC";
        t.c()   = "cc -O3 -DNDEBUG -fPIC";
        return t;
    }

    static Target Debug() {
        Target t;
        t.id()  = "debug";
        t.cpp() = "c++ -g -fPIC";
        t.c()   = "cc -g -fPIC";
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

    cpx::Tag<std::unordered_map<std::string, std::string>> vars = "toml,json:`vars,skipmissing,omitempty`";
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
void compile_multi(const std::string &name, const std::vector<CompileCommand> &commands);

struct Project {
    using Dep = std::variant<std::string, Dependency>;

    struct Targets {
        cpx::Tag<Target> release = {"toml,json:`release,skipmissing`", Target::Release()};
        cpx::Tag<Target> debug   = {"toml,json:`debug,skipmissing`", Target::Debug()};
    };
    cpx::Tag<Targets> targets = "toml,json:`targets,skipmissing`";

    cpx::Tag<std::unordered_map<std::string, Project>> packages = "toml:`packages,skipmissing,omitempty`";
    cpx::Tag<Package>                                  package  = "toml,json:`package`";

    cpx::Tag<std::unordered_map<std::string, Dep>> dependencies = "toml,json:`dependencies,skipmissing,omitempty`";
    cpx::Tag<Lib>                                  lib          = "toml,json:`lib,skipmissing`";
    cpx::Tag<std::unordered_map<std::string, std::vector<std::string>>> features = "toml,json:`features,skipmissing,omitempty`";

    cpx::Tag<std::string> cache               = "opt:`cache,env=CARTON_CACHE,skipmissing`";
    cpx::Tag<bool>        no_default_features = "opt:`no-default-features,help=Disable default features`";

    enum class LogLevel {
        trace,
        debug,
        info,
        warn,
        err,
        critical,
        off,
    };
    cpx::Tag<LogLevel> log_level = {"opt:`log-level,skipmissing`", LogLevel::warn};

    cpx::Tag<std::vector<CompileCommand>> compile_commands = "json:`compile_commands`";

    std::unordered_map<std::string, Project> *ppackages = nullptr;

    void build(const std::vector<std::string> &features = {}, bool subpackage = false);

private:
    void apply_package_placeholders();
    void resolve_remote_dep(const std::string &name, Dependency &dep);
    void collect_meta(const std::string &name, Dependency &dep);
};

Dependency &convert_dep(Project::Dep &dep);

std::string resolve_path(const std::string &cache, const std::string &path);
std::string git_clone(const std::string &cache, const std::string &git, const std::string &tag);

std::vector<std::string> expand_path(const std::string &working_dir, std::vector<std::string> sources);

void push_unique(std::vector<std::string> &vec, const std::string &value);
void push_unique(std::vector<std::string> &vec, const std::vector<std::string> &values);
