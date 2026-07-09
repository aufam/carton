module;

#include <string>
#include <vector>
#include <map>

export module carton:mod;

export {
    auto sort_modules(
        const std::string                               &working_dir, //
        std::vector<std::string>                        &sources,
        std::map<std::string, std::vector<std::string>> &mods
    ) -> std::vector<std::string>;
    auto collect_module_deps(const std::string &working_dir, const std::string &source) -> std::vector<std::string>;

    std::vector<std::string> sort_modules_p1689(
        const std::string                               &working_dir,
        std::vector<std::string>                        &files,
        std::vector<std::string>                        &ccs,
        std::map<std::string, std::vector<std::string>> &mods
    );
}
