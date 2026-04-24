#include "main.h"

#include <fmt/ranges.h>
#include <fmt/color.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <cpx/toml/toruniina_toml.h>

namespace fs = std::filesystem;

namespace {
    using namespace cpx;

    struct Signature {
        Tag<std::string> cmd  = "toml:`cmd`";
        Tag<std::string> file = "toml:`file`";
        Tag<std::string> deps = "toml:`deps`";
    };

    constexpr auto toml_version = cpx::toml::toruniina_toml::spec::v(1, 1, 0);

    std::unordered_map<std::string, Signature> toml_parse(const fs::path &signature_path) {
        if (!fs::exists(signature_path))
            return {};
        return cpx::toml::toruniina_toml::parse_from_file<std::unordered_map<std::string, Signature>>(
            signature_path.string(), toml_version
        );
    }
    void toml_dump(const std::unordered_map<std::string, Signature> &signature, const fs::path &signature_path) {
        auto          tml = cpx::toml::toruniina_toml::dump(signature, toml_version);
        std::ofstream out(signature_path);
        out << tml;
    }
} // namespace

// Keep the depfile parser exactly as-is, aside from includes already present.
static std::vector<std::string> parse_depfile(const std::string &path) {
    std::ifstream in(path);
    if (!in)
        return {};

    std::vector<std::string> result;

    std::string line, content;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\\') {
            line.pop_back();
            content += line;
            continue;
        } else {
            content += line;
        }

        // remove "target:"
        auto colon = content.find(':');
        if (colon == std::string::npos) {
            content.clear();
            continue;
        }

        std::string deps = content.substr(colon + 1);

        std::istringstream iss(deps);
        std::string        dep;
        while (iss >> dep) {
            result.push_back(dep);
        }
        content.clear();
    }

    return result;
}

// If you need a hash function, here's simple pseudo-code implemented in C++.
// Replace with a real hash (xxhash/sha1/etc.) if you want stronger properties.
static uint64_t hash64_pseudo(std::string_view s) {
    uint64_t h = 1469598103934665603ull;
    for (unsigned char c : s) {
        h ^= (uint64_t)c;
        h *= 1099511628211ull;
    }
    return h;
}

static uint64_t hash64_combine(uint64_t h, uint64_t v) {
    // Simple combiner (pseudo). Order-sensitive.
    // (This is a common-ish pattern; exact constants aren't critical for a build signature.)
    h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    return h;
}

static std::string hex64(uint64_t v) {
    std::ostringstream oss;
    oss << std::hex << v;
    return oss.str();
}

static fs::path signature_path_for(const fs::path &directory) {
    // One signature file per compile directory, containing many tables keyed by CompileCommand::file().
    return directory / ".rebuild.lock.toml";
}

// parse_depfile(depfile) returns a list of ABSOLUTE header paths.
// Hash each dependency entry independently (path + mtime) and combine.
static std::string hash_deps_snapshot_hex(const fs::path &depfile_abs) {
    std::vector<std::string> deps = parse_depfile(depfile_abs.string());

    // Make stable across depfile ordering differences (optional but usually desirable).
    std::sort(deps.begin(), deps.end());

    uint64_t h = 1469598103934665603ull; // seed

    for (const auto &dep_str : deps) {
        fs::path p = fs::path(dep_str).lexically_normal();

        // Per your note, these should already be absolute.
        // If something unexpected is relative, keep it deterministic anyway.
        const std::string p_str = p.string();

        uint64_t entry_h = hash64_pseudo(p_str);

        if (!fs::exists(p)) {
            entry_h = hash64_combine(entry_h, hash64_pseudo("MISSING"));
        } else {
            std::error_code ec;
            auto            t = fs::last_write_time(p, ec);
            if (ec) {
                entry_h = hash64_combine(entry_h, hash64_pseudo("NO_MTIME"));
            } else {
                auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t.time_since_epoch()).count();
                entry_h = hash64_combine(entry_h, hash64_pseudo(std::to_string(ns)));
            }
        }

        // Combine into overall deps hash
        h = hash64_combine(h, entry_h);
    }

    return hex64(h);
}

static Signature
make_signature(const fs::path &directory, const fs::path &output, const fs::path &depfile, const std::string &command_string) {
    const auto out_abs = (output.is_absolute() ? output : (directory / output)).lexically_normal();
    const auto dep_abs = (depfile.is_absolute() ? depfile : (directory / depfile)).lexically_normal();

    Signature s;
    s.cmd() = hex64(hash64_pseudo(command_string));

    // "file" field: hashed target identity. Here we use the resolved output path string.
    // If you'd rather hash CompileCommand::file(), change this accordingly.
    s.file() = hex64(hash64_pseudo(out_abs.string()));

    // deps: hash each dep independently, then combine
    s.deps() = hash_deps_snapshot_hex(dep_abs);
    return s;
}

static bool needs_rebuild(
    const std::unordered_map<std::string, Signature> &sig_map,
    const fs::path                                   &directory,
    const fs::path                                   &output,
    const fs::path                                   &depfile,
    const std::string                                &command_string,
    const std::string                                &file_key /* CompileCommand::file() */
) {
    const auto out_abs = (output.is_absolute() ? output : (directory / output)).lexically_normal();
    const auto dep_abs = (depfile.is_absolute() ? depfile : (directory / depfile)).lexically_normal();

    // If output or depfile is missing, we must rebuild.
    if (!fs::exists(out_abs) || !fs::exists(dep_abs))
        return true;


    // Load prior signatures; if none, rebuild.
    auto it = sig_map.find(file_key);
    if (it == sig_map.end())
        return true;

    const Signature  current = make_signature(directory, output, depfile, command_string);
    const Signature &old     = it->second;

    if (old.cmd() != current.cmd())
        return true;

    if (old.file() != current.file())
        return true;

    if (old.deps() != current.deps())
        return true;

    return false; // signature matches => up-to-date
}

void compile_multi(const std::string &name, const std::vector<CompileCommand> &commands) {
    if (commands.empty())
        return;

    auto &dir = commands[0].directory();
    for (const auto &cmd : commands)
        if (dir != cmd.directory())
            throw std::runtime_error("All CompileCommands in compile_multi must have the same directory");

    const fs::path sig_path = signature_path_for(dir);

    std::unordered_map<std::string, Signature> sig_map = toml_parse(sig_path);

    std::vector<const CompileCommand *> needs_rebuild_ptrs;
    for (const auto &cmd : commands)
        if (needs_rebuild(sig_map, cmd.directory(), cmd.output(), cmd.depfile(), cmd.command(), cmd.file()))
            needs_rebuild_ptrs.push_back(&cmd);

    if (needs_rebuild_ptrs.empty())
        return; // all up-to-date

    fmt::print(fmt::emphasis::bold | fmt::fg(fmt::color::green), "{:>12} ", "Compiling");
    fmt::println("{} ({} files)", name, needs_rebuild_ptrs.size());
    // TODO: parallelize this
    for (const auto *cmd : needs_rebuild_ptrs) {
        const std::string full_cmd = fmt::format(
            "mkdir -p \"{0}\" && cd \"{0}\" && "
            "(mkdir -p \"{1}\" && {2})",
            cmd->directory(),
            fs::path(cmd->output()).parent_path().string(),
            cmd->command()
        );

        spdlog::info("compiling: cmd={:?}", full_cmd);
        if (std::system(full_cmd.c_str()) != 0) {
            throw std::runtime_error(fmt::format("Failed to compile {}: command={:?}", cmd->file(), full_cmd));
        }

        // On success, update this file() entry in the directory-level signature TOML.
        sig_map[cmd->file()] =
            make_signature(fs::path(cmd->directory()), fs::path(cmd->output()), fs::path(cmd->depfile()), cmd->command());

        toml_dump(sig_map, sig_path);
    }
}
