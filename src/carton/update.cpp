module;

#include <reproc++/run.hpp>

module carton;

int Carton::Update() {
    // TODO: custom registry path
    auto [ret, ec] = reproc::run(std::vector<std::string_view>{"sh", "-c", R"sh(
        set -e
        mkdir -p ~/.carton
        curl -fsLo ~/.carton/registry.toml https://raw.githubusercontent.com/aufam/carton/main/registry.toml
    )sh"});

    return ret;
}
