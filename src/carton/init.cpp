module;

#include <reproc++/run.hpp>
#include <spdlog/spdlog.h>

module carton;
import cpx.toruniina_toml;

int Carton::Init(Package &args) {
    std::string filename = "carton.toml";
    if (fs::exists(filename)) {
        spdlog::error("{} already exists", filename);
        return 1;
    }

    std::ofstream of(filename);
    if (!of) {
        spdlog::error("Cannot create {}", filename);
        return 1;
    }

    std::tuple carton = {cpx::field_ref(args) = "package"};
    if (auto &v = args.version; v.empty())
        v = "0.1.0";

    // TODO
    of << "#:schema https://raw.githubusercontent.com/aufam/carton/main/carton-schema.json\n\n";
    of << cpx::toruniina_toml::io << carton;
    of << "[dependencies]\n";

    auto [ret, ec] = reproc::run(std::vector<std::string_view>{"sh", "-c", R"sh(
        set -e
        mkdir -p src/
        if [ ! -f src/main.cpp ]; then
            echo '#include <iostream>' >> src/main.cpp
            echo '' >> src/main.cpp
            echo 'int main() {' >> src/main.cpp
            echo '    std::cout << "Hello, world!" << std::endl;' >> src/main.cpp
            echo '}' >> src/main.cpp
        fi
        if [ ! -f .gitignore ]; then
            curl -fsSLo .gitignore https://raw.githubusercontent.com/aufam/carton/main/.gitignore
        fi
        if [ ! -f .clang-format ]; then
            curl -fsSLo .clang-format https://raw.githubusercontent.com/aufam/carton/main/.clang-format
        fi
    )sh"});

    return ret;
}
