module;

#include <cpx/reflect.h>
#include <string>
#include <optional>
#include <vector>

export module carton:dependency;

export struct Dependency {
    std::string name;
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

    std::optional<int> cpp_standard = std::nullopt;

    Dependency &operator+=(const Dependency &other);
    bool        empty() const;
    std::string display_name() const;
};

template <>
struct cpx::Reflect<Dependency> //
    : Fields<
          Reflect<Dependency>,
          &Dependency::name,
          &Dependency::version,
          &Dependency::path,
          &Dependency::url,
          &Dependency::git,
          &Dependency::branch,
          &Dependency::tag,
          &Dependency::subdir,
          &Dependency::features,
          &Dependency::optional,
          &Dependency::default_features,
          &Dependency::src,
          &Dependency::inc,
          &Dependency::lib,
          &Dependency::mod,
          &Dependency::flags,
          &Dependency::link_flags,
          &Dependency::pre> {

    static constexpr TagInfo name             = "name,skipmissing";
    static constexpr TagInfo version          = "version,oneof=version|path|url|git";
    static constexpr TagInfo path             = "path,oneof=version|path|url|git";
    static constexpr TagInfo url              = "url,oneof=version|path|url|git";
    static constexpr TagInfo git              = "git,oneof=version|path|url|git";
    static constexpr TagInfo branch           = "branch,oneof=branch|tag";
    static constexpr TagInfo tag              = "tag,oneof=branch|tag";
    static constexpr TagInfo subdir           = "subdir,skipmissing";
    static constexpr TagInfo features         = "features,skipmissing";
    static constexpr TagInfo optional         = "optional,skipmissing";
    static constexpr TagInfo default_features = "default-features,skipmissing";
    static constexpr TagInfo src              = "src,skipmissing";
    static constexpr TagInfo inc              = "inc,skipmissing";
    static constexpr TagInfo lib              = "lib,skipmissing";
    static constexpr TagInfo mod              = "mod,skipmissing";
    static constexpr TagInfo flags            = "flags,skipmissing";
    static constexpr TagInfo link_flags       = "link-flags,skipmissing";
    static constexpr TagInfo pre              = "pre,skipmissing";

    static constexpr tags_type tags() {
        return std::tie(
            name,
            version,
            path,
            url,
            git,
            branch,
            tag,
            subdir,
            features,
            optional,
            default_features,
            src,
            inc,
            lib,
            mod,
            flags,
            link_flags,
            pre
        );
    }
};
