module;

#include <cpx/reflect.h>
#include "macro.h"

export module carton:config;
import :profile;
import cpx;

export struct Config {
    Profiles profiles;
};

// clang-format off
CPX_REFLECT(
    (Config, ),

    ((profiles, "profile, skipmissing"))
);
// clang-format on
