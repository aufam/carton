module;

#include <cpx/fmt.h>
#include <fmt/color.h>
#include <spdlog/spdlog.h>
#include <reproc++/run.hpp>
#include <filesystem>

module carton;

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

    print_status("Running", exe);
    auto [status, ec] = reproc::run(std::vector<std::string>{"sh", "-c", exe});
    if (ec)
        return -1;

    if (status >= 0)
        return status;

    return 256;
}
