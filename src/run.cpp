module;

#include <cpx/fmt.h>
#include <fmt/color.h>
#include <spdlog/spdlog.h>
#include <filesystem>

module carton;

namespace fs = std::filesystem;

int Carton::run(const Cache::Meta &m) {
    const auto output = fs::path(m.build_dir) / m.lib.name;

    std::string out = output.string();
    for (size_t pos = 0; (pos = out.find(' ', pos)) != std::string::npos;) {
        out.replace(pos, 1, "\\ ");
        pos += 2;
    }

    std::string exe = f("{} {}", out, fmt::join(cli->run->args, " "));
    if (cli->run->args.empty())
        exe.pop_back();

    fmt::print(stderr, fmt::emphasis::bold | fmt::fg(fmt::terminal_color::green), "{:>12} ", "Running");
    fmt::println(stderr, "`{}`", exe);
    int ret = std::system(exe.c_str());
    if (ret == -1) {
        return -1;
    } else if (WIFEXITED(ret)) {
        return WEXITSTATUS(ret);
    } else {
        return 256;
    }
}
