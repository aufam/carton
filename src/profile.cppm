module;

#include <cpx/reflect.h>
#include <string>
#include <vector>

export module carton:profile;

export struct Profile {
    std::string              id;
    std::string              cxx       = "c++";
    std::string              c         = "cc";
    std::string              modules   = "auto";
    bool                     debug     = false;
    bool                     asan      = false;
    bool                     lto       = false;
    int                      opt_level = 0;
    std::vector<std::string> flags;
    std::vector<std::string> link_flags;

    bool _module_support = false;

    static Profile Release() {
        Profile t;
        t.id        = "release";
        t.debug     = false;
        t.lto       = false;
        t.asan      = false;
        t.opt_level = 3;
        t.flags     = {"-fPIC", "-Wall", "-Wextra"};
        return t;
    }

    static Profile Dev() {
        Profile t;
        t.id        = "dev";
        t.debug     = true;
        t.lto       = false;
        t.asan      = true;
        t.opt_level = 0;
        t.flags     = {"-fPIC", "-Wall", "-Wextra"};
        return t;
    }
};

template <>
struct cpx::Reflect<Profile> //
    : Fields<
          Reflect<Profile>,
          &Profile::id,
          &Profile::cxx,
          &Profile::c,
          &Profile::modules,
          &Profile::debug,
          &Profile::asan,
          &Profile::lto,
          &Profile::opt_level,
          &Profile::flags,
          &Profile::link_flags> {
    static constexpr TagInfo id         = "id";
    static constexpr TagInfo cxx        = "cxx,skipmissing";
    static constexpr TagInfo c          = "c,skipmissing";
    static constexpr TagInfo modules    = "modules,skipmissing";
    static constexpr TagInfo debug      = "debug,skipmissing";
    static constexpr TagInfo asan       = "asan,skipmissing";
    static constexpr TagInfo lto        = "lto,skipmissing";
    static constexpr TagInfo opt_level  = "opt-level,skipmissing";
    static constexpr TagInfo flags      = "flags,skipmissing";
    static constexpr TagInfo link_flags = "link-flags,skipmissing";

    static constexpr tags_type tags() {
        return std::tie(id, cxx, c, modules, debug, asan, lto, opt_level, flags, link_flags);
    }
};
