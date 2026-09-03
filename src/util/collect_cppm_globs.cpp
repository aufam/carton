module;

#include <string>
#include <vector>

module carton;

std::vector<std::string>
collect_cppm_globs(const fs::path &working_dir, const fs::path &src_dir, std::vector<std::string> *cpps) {
    std::vector<std::string> result;

    auto it = fs::recursive_directory_iterator(working_dir / src_dir);
    for (auto ptr = fs::begin(it); ptr != fs::end(it); ++ptr) {
        const auto &entry = *ptr;
        if (entry.path().filename() != "lib.cppm")
            continue;

        auto relative_dir = fs::relative(entry.path().parent_path(), working_dir);
        result.push_back((relative_dir / "*.cppm").generic_string());
        if (cpps)
            cpps->push_back((relative_dir / "*").generic_string());
    }

    return result;
}
