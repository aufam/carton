module;

#include <cpx/reflect.h>

export module carton:config;
import :profile;
import cpx;

export struct Config {
    Profiles profiles;
    // TODO: do we need other things?

    static constexpr std::tuple __field_tags__{
        cpx::field<&Config::profiles> = "profile, skipmissing",
    };
};
