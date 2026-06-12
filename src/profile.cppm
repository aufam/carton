module;

#include <cpx/reflect.h>
#include <string>
#include <vector>
#include "macro.h"

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

// clang-format off
CPX_REFLECT(
    (Profile, ),

    ((id         , "id        , skipmissing"))
    ((cxx        , "cxx       , skipmissing"))
    ((c          , "c         , skipmissing"))
    ((modules    , "modules   , skipmissing"))
    ((debug      , "debug     , skipmissing"))
    ((asan       , "asan      , skipmissing"))
    ((lto        , "lto       , skipmissing"))
    ((opt_level  , "opt-level , skipmissing"))
    ((flags      , "flags     , skipmissing"))
    ((link_flags , "link-flags, skipmissing"))
);
// clang-format on
