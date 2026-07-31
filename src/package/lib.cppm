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
    std::vector<std::string> versions;

    static constexpr std::tuple __field_tags__ = {
        cpx::field<&Package::name>          = "name,positional",
        cpx::field<&Package::version>       = "version,skipmissing,omitempty",
        cpx::field<&Package::edition>       = "edition,skipmissing,omitempty",
        cpx::field<&Package::authors>       = "authors,skipmissing,omitempty",
        cpx::field<&Package::description>   = "description,skipmissing,omitempty",
        cpx::field<&Package::license>       = "license,skipmissing,omitempty",
        cpx::field<&Package::license_file>  = "license-file,skipmissing,omitempty",
        cpx::field<&Package::readme>        = "readme,skipmissing,omitempty",
        cpx::field<&Package::repository>    = "repository,skipmissing,omitempty",
        cpx::field<&Package::homepage>      = "homepage,skipmissing,omitempty",
        cpx::field<&Package::documentation> = "documentation,skipmissing,omitempty",
        cpx::field<&Package::keywords>      = "keywords,skipmissing,omitempty",
        cpx::field<&Package::versions>      = "versions,skipmissing,omitempty",
    };
};
