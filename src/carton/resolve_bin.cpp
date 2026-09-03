module;

#include <string>
#include <vector>

module carton;

void Carton::resolve_bin() {
    if (bins.empty()) {
        const fs::path root = lib.path;
        if (fs::path main_path = root / "src" / "main.cpp"; fs::exists(main_path)) {
            Binary main;
            main.name = "main";
            main.path = main_path.string();
            bins.push_back(main);
            resolve_bin();
        }
    }

    for (auto &bin : bins) {
        const fs::path path = bin.path;
        if (fs::is_directory(path)) {
            bin.src = {bin.path + "/*"};
        } else {
            bin.src = {bin.path};
        }
        expand_path(lib.path, bin.src);
    }
}
