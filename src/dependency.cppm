module;

#include <cpx/reflect.h>
#include <cpx/toml/toruniina_toml.h>
#include <string>
#include <optional>
#include <vector>
#include "macro.h"

export module carton:dependency;

export struct Dependency {
    std::string version;
    std::string path;
    std::string url;
    std::string git;
    std::string branch;
    std::string tag;
    std::string subdir;

    std::vector<std::string> features;
    bool                     optional;
    std::optional<bool>      default_features;

    std::vector<std::string> src;
    std::vector<std::string> inc;
    std::vector<std::string> lib;
    std::vector<std::string> mod;
    std::vector<std::string> flags;
    std::vector<std::string> link_flags;
    std::string              pre;

    std::string        name;
    std::optional<int> cpp_standard = std::nullopt;

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
        } else {
            name += " (" + path + ")";
        }
        return name;
    }
};

// clang-format off
CPX_REFLECT(
    (Dependency, ),

    ((version         , "version         , oneof=version|path|url|git"))
    ((path            , "path            , oneof=version|path|url|git"))
    ((url             , "url             , oneof=version|path|url|git"))
    ((git             , "git             , oneof=version|path|url|git"))
    ((branch          , "branch          , oneof=branch|tag          "))
    ((tag             , "tag             , oneof=branch|tag          "))

    ((subdir          , "subdir          , skipmissing               "))
    ((features        , "features        , skipmissing               "))
    ((optional        , "optional        , skipmissing               "))
    ((default_features, "default-features, skipmissing               "))

    ((src             , "src             , skipmissing               "))
    ((inc             , "inc             , skipmissing               "))
    ((lib             , "lib             , skipmissing               "))
    ((mod             , "mod             , skipmissing               "))
    ((flags           , "flags           , skipmissing               "))
    ((link_flags      , "link-flags      , skipmissing               "))
    ((pre             , "pre             , skipmissing               "))
);
// clang-format on

template <>
struct cpx::toml::Reflect<Dependency> : cpx::Reflect<Dependency> {
    /* serialize */
    using const_type = Fields::const_type;

    static constexpr const_type of(const Dependency &d) {
        return Fields::of(d);
    }

    /* deserialize */
    using fields_type = Fields::type;

    class type {
    public:
        explicit constexpr type(Dependency &d)
            : version(d.version)
            , fields(Fields::of(d)) {}

        std::string &version;
        fields_type  fields;
    };

    static constexpr type of(Dependency &d) {
        return type(d);
    }
};

template <>
struct cpx::serde::Deserialize<__toml11::value, cpx::toml::Reflect<Dependency>::type> {
    const __toml11::value &node;

    void into(cpx::toml::Reflect<Dependency>::type &v) {
        if (node.is_string())
            v.version = node.as_string(std::nothrow);
        else
            Deserialize<__toml11::value, cpx::toml::Reflect<Dependency>::fields_type>{node}.into(v.fields);
    }
};
