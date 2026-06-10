module;

#include <cpx/reflect.h>
#include <string>
#include <vector>

export module carton:package;

export struct Package {
    std::string              name;
    std::string              version;
    int                      edition = 17;
    std::vector<std::string> authors;
    std::string              description;
    std::string              license;
};

template <>
struct cpx::Reflect<Package> //
    : Fields<
          Reflect<Package>, //
          &Package::name,
          &Package::version,
          &Package::edition,
          &Package::authors,
          &Package::description,
          &Package::license> {
    static constexpr TagInfo name        = "name";
    static constexpr TagInfo version     = "version,skipmissing";
    static constexpr TagInfo edition     = "edition,skipmissing";
    static constexpr TagInfo authors     = "authors,skipmissing";
    static constexpr TagInfo description = "description,skipmissing";
    static constexpr TagInfo license     = "license,skipmissing";

    static constexpr tags_type tags() {
        return std::tie(name, version, edition, authors, description, license);
    }
};
