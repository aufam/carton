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
    std::string              license_file;
    std::string              readme;
    std::string              repository;
    std::string              homepage;
    std::string              documentation;
    std::vector<std::string> keywords;
};

// clang-format off
CPX_REFLECT(
    (Package, ),
    ((name,          "name"))
    ((version,       "version,skipmissing"))
    ((edition,       "edition,skipmissing"))
    ((authors,       "authors,skipmissing"))
    ((description,   "description,skipmissing"))
    ((license,       "license,skipmissing"))
    ((license_file,  "license-file,skipmissing"))
    ((readme,        "readme,skipmissing"))
    ((repository,    "repository,skipmissing"))
    ((homepage,      "homepage,skipmissing"))
    ((documentation, "documentation,skipmissing"))
    ((keywords,      "keywords,skipmissing"))
);
// clang-format on
