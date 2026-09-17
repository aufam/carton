module;

#include <string>
#include <vector>
#include <filesystem>

module carton;

std::vector<std::string>
collect_cppm_globs(const fs::path &working_dir, const fs::path &src_dir, std::vector<std::string> *cpps) {
    std::vector<std::string> result;

    const auto root = working_dir / src_dir;

    auto add_dir = [&](const fs::path &dir) {
        if (!fs::exists(dir / "lib.cppm"))
            return;

        const auto relative_dir = fs::relative(dir, working_dir);

        result.push_back((relative_dir / "*.cppm").generic_string());

        if (cpps)
            cpps->push_back((relative_dir / "*").generic_string());
    };

    // src/lib.cppm
    add_dir(root);

    // src/*/lib.cppm
    for (const auto &entry : fs::directory_iterator(root)) {
        if (entry.is_directory())
            add_dir(entry.path());
    }

    return result;
}
