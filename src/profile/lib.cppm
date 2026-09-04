module;

#include <string>
#include <vector>

export module carton:profile;
import cpx;

export struct Profile {
    std::string              name;
    std::string              cxx       = "c++";
    std::string              c         = "cc";
    std::string              ar        = "ar";
    std::string              arch      = "native";
    std::string              modules   = "auto";
    bool                     debug     = false;
    bool                     lto       = false;
    int                      opt_level = 0;
    std::vector<std::string> sanitize;
    std::vector<std::string> flags;
    std::vector<std::string> link_flags;

    static constexpr std::tuple __field_tags__{
        cpx::field<&Profile::cxx>        = "cxx",
        cpx::field<&Profile::c>          = "c",
        cpx::field<&Profile::ar>         = "ar        , skipmissing",
        cpx::field<&Profile::arch>       = "arch      , skipmissing",
        cpx::field<&Profile::modules>    = "modules   , skipmissing",
        cpx::field<&Profile::debug>      = "debug     , skipmissing",
        cpx::field<&Profile::sanitize>   = "sanitize  , skipmissing",
        cpx::field<&Profile::lto>        = "lto       , skipmissing",
        cpx::field<&Profile::opt_level>  = "opt-level , skipmissing",
        cpx::field<&Profile::flags>      = "flags     , skipmissing",
        cpx::field<&Profile::link_flags> = "link-flags, skipmissing",
    };

    bool        _module_support = false;
    std::string _module_compiler;

    static Profile Release() {
        Profile t;
        t.name      = "release";
        t.debug     = false;
        t.lto       = true;
        t.opt_level = 3;
        t.flags     = {"-fPIC", "-Wall", "-Wextra"};
        return t;
    }

    static Profile Dev() {
        Profile t;
        t.name      = "dev";
        t.debug     = true;
        t.lto       = false;
        t.opt_level = 0;
        t.sanitize  = {"address", "undefined"};
        t.flags     = {"-fPIC", "-Wall", "-Wextra"};
        return t;
    }
};

export struct Profiles {
    Profile release = Profile::Release();
    Profile dev     = Profile::Dev();

    static constexpr std::tuple __field_tags__{
        cpx::field<&Profiles::release> = "release, skipmissing",
        cpx::field<&Profiles::dev>     = "dev    , skipmissing",
    };

    void check_module_support();
};
