module;

#include <cpx/reflect.h>
#include "../macro.h"

export module carton:config;
import :profile;
import cpx;

export struct Config {
    Profiles profiles;
    // TODO: do we need other things?
};

// clang-format off
CPX_REFLECT(
    (Config, ),

    ((profiles, "profile, skipmissing"))
);
// clang-format on
