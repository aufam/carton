module;

#include <reproc++/run.hpp>
#include <spdlog/spdlog.h>

module carton;

int Carton::Update() {
    // TODO: custom registry path
    auto [ret, ec] = reproc::run(std::vector<std::string_view>{"sh", "-c", R"sh(
        set -e
        mkdir -p ~/.carton
        wget -q https://raw.githubusercontent.com/aufam/carton/main/registry.toml -O ~/.carton/registry.toml
    )sh"});

    return ret;
}
