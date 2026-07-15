module;

#include <string>
#include <vector>

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

    static constexpr std::tuple __field_tags__{
        cpx::field<&Package::name>          = "name",
        cpx::field<&Package::version>       = "version,skipmissing",
        cpx::field<&Package::edition>       = "edition,skipmissing",
        cpx::field<&Package::authors>       = "authors,skipmissing",
        cpx::field<&Package::description>   = "description,skipmissing",
        cpx::field<&Package::license>       = "license,skipmissing",
        cpx::field<&Package::license_file>  = "license-file,skipmissing",
        cpx::field<&Package::readme>        = "readme,skipmissing",
        cpx::field<&Package::repository>    = "repository,skipmissing",
        cpx::field<&Package::homepage>      = "homepage,skipmissing",
        cpx::field<&Package::documentation> = "documentation,skipmissing",
        cpx::field<&Package::keywords>      = "keywords,skipmissing",
    };
};
