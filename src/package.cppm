module;

#include <string>
#include <vector>
#include "macro.h"

export module carton:package;
import cpx;

export struct Package {
    std::string              name;
    std::string              version;
    int                      edition = 17;
    std::vector<std::string> authors;
    std::string              description;
    std::string              license;
};

// clang-format off
CPX_REFLECT(
    (Package, ),
    ((name        , "name"))
    ((version     , "version,skipmissing"))
    ((edition     , "edition,skipmissing"))
    ((authors     , "authors,skipmissing"))
    ((description , "description,skipmissing"))
    ((license     , "license,skipmissing"))
);
// clang-format on
