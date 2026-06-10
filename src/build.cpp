module;

#include <cpx/fmt.h>
#include <fmt/color.h>
#include <spdlog/spdlog.h>
#include <filesystem>

module carton;

namespace fs = std::filesystem;

std::pair<bool, Cache::Meta> Carton::build(const Profile &profile, std::vector<CompileCommand> &ccs, bool do_build) {
    const auto start = std::chrono::system_clock::now();

    bool relink = false;
    auto hash   = std::unordered_map<std::string, std::string>();
    for (const auto &m : cache->meta) {
        const auto name = m.lib.display_name();
        ccs.insert(ccs.end(), m.precompile_commands.begin(), m.precompile_commands.end());
        ccs.insert(ccs.end(), m.compile_commands.begin(), m.compile_commands.end());
        relink |= CompileCommand::compile_multi(name, m.precompile_commands, hash, true);
    }

    auto       m    = collect_meta(profile, lib);
    const auto name = m.lib.display_name();
    ccs.insert(ccs.end(), m.precompile_commands.begin(), m.precompile_commands.end());
    ccs.insert(ccs.end(), m.compile_commands.begin(), m.compile_commands.end());
    relink |= CompileCommand::compile_multi(name, m.precompile_commands, hash, true);

    if (!do_build)
        return std::make_pair(relink, std::move(m));

    for (const auto &m : cache->meta) {
        const auto name = m.lib.display_name();
        relink |= CompileCommand::compile_multi(name, m.compile_commands, hash);
    }

    relink |= CompileCommand::compile_multi(name, m.compile_commands, hash);

    const auto output = fs::path(m.build_dir) / m.lib.name;
    if (!m.main_o.empty() && (relink || !fs::exists(output))) {
        const auto LINK =
            f("{} {} {} {}",
              profile.cxx,
              profile.lto ? "-flto" : "",
              profile.asan ? "-fsanitize=address,undefined" : "",
              fmt::join(profile.link_flags, " "));

        fs::create_directories(output.parent_path());
        fmt::print(stderr, fmt::emphasis::bold | fmt::fg(fmt::terminal_color::green), "{:>12} ", "Linking");
        fmt::println(stderr, "{}", lib.display_name());

        auto link_cmd = f("{} -o '{}' {} {}", LINK, output.string(), fmt::join(m.link_flags, " "), m.main_o);
        spdlog::debug("linking cmd={}", link_cmd);
        if (std::system(link_cmd.c_str()) != 0)
            throw ferr("linking failed: name={}, cmd=`{}`", m.lib.name, link_cmd);
    }

    std::string profile_info = profile.opt_level == 0 ? "unoptimized" : "optimized";
    if (profile.debug)
        profile_info += " + debuginfo";
    if (profile.asan)
        profile_info += " + asan";

    std::chrono::duration<double> elapsed = std::chrono::system_clock::now() - start;
    fmt::print(stderr, fmt::emphasis::bold | fmt::fg(fmt::terminal_color::green), "{:>12} ", "Finished");
    fmt::println(stderr, "`{}` profile [{}] target(s) in {:.2f}", profile.id, profile_info, elapsed.count());

    return std::make_pair(relink, std::move(m));
}
