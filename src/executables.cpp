#include "main.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;
void collect_executables(const Profile &profile, const Project &project) {
    std::unordered_map<std::string, Executable> examples, tests, benches, bins;

    auto base = fs::current_path() / project.lib().path() / project.lib().subdir();

    auto collect = [](const fs::path &dir, std::unordered_map<std::string, Executable> &out) {
        if (!fs::exists(dir) || !fs::is_directory(dir))
            return;

        for (const auto &entry : fs::directory_iterator(dir)) {
            const auto &path = entry.path();
            auto        name = path.stem().string();

            if (fs::is_regular_file(path) && path.extension() == ".cpp") {
                auto &exe  = out[name];
                exe.name() = name;
                exe.src()  = {path.string()};
            } else if (fs::is_directory(path)) {
                auto &exe  = out[name];
                exe.name() = name;

                for (const auto &sub : fs::recursive_directory_iterator(path)) {
                    if (fs::is_regular_file(sub.path()) && sub.path().extension() == ".cpp") {
                        exe.src().push_back(sub.path().string());
                    }
                }

                // optional: deterministic build order
                std::sort(exe.src().begin(), exe.src().end());
            }
        }
    };

    collect(base / "examples", examples);
    collect(base / "tests", tests);
    collect(base / "benches", benches);
    if (project.bin().empty()) {
        collect(base / "benches", benches);
    } else {
        for (auto &bin : project.bin())
            bins[bin.name()] = bin;
    }

    // TODO: attach to project/profile if needed
    // e.g. project.set_examples(std::move(examples));
}
