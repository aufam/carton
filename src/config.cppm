module;

#include <cpx/reflect.h>
#include "macro.h"

export module carton:config;
import :profile;

export struct Config {
    struct Profiles {
        Profile release = Profile::Release();
        Profile dev     = Profile::Dev();
    };

    Profiles profiles;
};

// clang-format off
CPX_REFLECT(
    (Config, ),

    ((profiles, "profiles, skipmissing"))
);

CPX_REFLECT(
    (Config::Profiles, ),

    ((release , "release, skipmissing"))
    ((dev     , "dev    , skipmissing"))
);
// clang-format on
