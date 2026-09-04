module;

#include <string>
#include <vector>
#include <spdlog/spdlog.h>

module carton;
import cpx.fmt;

void Carton::resolve_bin() {
    if (bins.empty()) {
        const fs::path root = lib.path;
        if (fs::path main_path = root / "src" / "main.cpp"; fs::exists(main_path)) {
            Binary main;
            main.name = "main";
            main.path = "src/main.cpp";
            bins.push_back(main);
            return resolve_bin();
        }
    }

    spdlog::trace("resolve_bin before bins={}", bins);
    for (auto &bin : bins) {
        const fs::path path = bin.path;
        if (fs::is_directory(path)) {
            bin.src = {bin.path + "/*"};
        } else {
            bin.src = {bin.path};
        }
        expand_path(lib.path, bin.src);
    }
    spdlog::trace("resolve_bin after bins={}", bins);
}
