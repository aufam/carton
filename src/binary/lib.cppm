module;

#include <string>
#include <vector>
#include <tuple>

export module carton:binary;
import cpx;

export struct Binary {
    std::string              name;
    std::string              path;
    std::vector<std::string> required_features;

    static constexpr std::tuple __field_tags__ = {
        cpx::field<&Binary::name>              = "name",
        cpx::field<&Binary::path>              = "path",
        cpx::field<&Binary::required_features> = "required-features,skipmissing",
    };

    static std::vector<Binary> Inspect(const std::string &working_dir);
};
