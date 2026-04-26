#include "main.h"

#include <fmt/ranges.h>
#include <fmt/color.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <cpx/toml/toruniina_toml.h>
#include <cpx/defer.h>
#include <xxhash.h>

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
static uint64_t hash64_pseudo(std::string_view s, XXH3_state_t *state = nullptr) {
    bool own = !state;
    if (own) {
        state = XXH3_createState();
        XXH3_64bits_reset(state);
    }
    XXH3_64bits_update(state, s.data(), s.size());
    uint64_t h = XXH3_64bits_digest(state);
    if (own)
        XXH3_freeState(state);
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
    return directory / "carton.lock";
}

static std::string hash_file_content_hex(const fs::path &p, std::unordered_map<std::string, std::string> &hash_history) {
    const std::string key = p.string();

    // cache hit
    if (auto it = hash_history.find(key); it != hash_history.end())
        return it->second;

    std::ifstream in(p, std::ios::binary);
    if (!in)
        return "MISSING";

    uint64_t h = 1469598103934665603ull;

    auto state = XXH3_createState();
    XXH3_64bits_reset(state);
    auto _ = defer([state]() { XXH3_freeState(state); });
    char buffer[4096];
    while (in.read(buffer, sizeof(buffer)) || in.gcount() > 0) {
        std::string_view chunk(buffer, in.gcount());
        h = hash64_combine(h, hash64_pseudo(chunk, state));
    }

    std::string result = hex64(h);
    hash_history[key]  = result;
    return result;
}

static std::string
hash_deps_snapshot_hex(const fs::path &depfile_abs, std::unordered_map<std::string, std::string> &hash_history) {
    std::vector<std::string> deps = parse_depfile(depfile_abs.string());
    std::sort(deps.begin(), deps.end());

    uint64_t h = 1469598103934665603ull;

    auto state = XXH3_createState();
    XXH3_64bits_reset(state);
    auto _ = defer([state]() { XXH3_freeState(state); });
    for (const auto &dep_str : deps) {
        fs::path p = fs::path(dep_str).lexically_normal();

        uint64_t entry_h;

        if (!fs::exists(p)) {
            entry_h = hash64_pseudo("MISSING", state);
        } else {
            std::string content_hash = hash_file_content_hex(p, hash_history);
            entry_h                  = hash64_pseudo(content_hash, state);
        }

        h = hash64_combine(h, entry_h);
    }

    return hex64(h);
}

static Signature make_signature(const CompileCommand &cc, std::unordered_map<std::string, std::string> &hash_history) {
    const fs::path directory = cc.directory();
    const fs::path depfile   = cc.depfile();
    const auto     dep_abs   = (depfile.is_absolute() ? depfile : (directory / depfile)).lexically_normal();

    Signature s;
    s.cmd() = hex64(hash64_pseudo(cc.command()));

    // hash SOURCE FILE CONTENT instead of path
    {
        fs::path file_abs =
            (fs::path(cc.file()).is_absolute() ? fs::path(cc.file()) : (directory / cc.file())).lexically_normal();

        s.file() = hash_file_content_hex(file_abs, hash_history);
    }

    s.deps() = hash_deps_snapshot_hex(dep_abs, hash_history);
    return s;
}

static bool needs_rebuild(
    const std::unordered_map<std::string, Signature> &sig_map,
    const CompileCommand                             &cc,
    std::unordered_map<std::string, std::string>     &hash_history
) {
    const fs::path output    = cc.output();
    const fs::path directory = cc.directory();
    const fs::path depfile   = cc.depfile();

    const auto out_abs = (output.is_absolute() ? output : (directory / output)).lexically_normal();
    const auto dep_abs = (depfile.is_absolute() ? depfile : (directory / depfile)).lexically_normal();

    if (!fs::exists(out_abs) || !fs::exists(dep_abs))
        return true;

    auto it = sig_map.find(cc.output());
    if (it == sig_map.end())
        return true;

    const Signature  current = make_signature(cc, hash_history);
    const Signature &old     = it->second;

    return old.cmd() != current.cmd() || old.file() != current.file() || old.deps() != current.deps();
}

bool compile_multi(
    const std::string                            &name,
    const std::vector<CompileCommand>            &commands,
    std::unordered_map<std::string, std::string> &hash_history
) {
    if (commands.empty())
        return false;

    auto &dir = commands[0].directory();
    for (const auto &cmd : commands)
        if (dir != cmd.directory())
            throw std::runtime_error("All CompileCommands in compile_multi must have the same directory");

    const fs::path sig_path = signature_path_for(dir);

    std::unordered_map<std::string, Signature> sig_map = toml_parse(sig_path);

    std::vector<const CompileCommand *> needs_rebuild_ptrs;
    for (const auto &cmd : commands)
        if (needs_rebuild(sig_map, cmd, hash_history))
            needs_rebuild_ptrs.push_back(&cmd);

    if (needs_rebuild_ptrs.empty())
        return false; // all up-to-date

    fmt::print(stderr, fmt::emphasis::bold | fmt::fg(fmt::color::green), "{:>12} ", "Compiling");
    fmt::println(stderr, "{} ({} files)", name, needs_rebuild_ptrs.size());
    // TODO: parallelize this
    for (const auto *cmd : needs_rebuild_ptrs) {
        const fs::path output = cmd->output();
        const fs::path outdir = output.is_absolute() ? output.parent_path() : (fs::path(dir) / output).parent_path();

        const std::string full_cmd = fmt::format(
            "mkdir -p \"{0}\" && "
            "cd \"{1}\" && {2}",
            outdir.string(),
            dir,
            cmd->command()
        );

        spdlog::info("compiling: cmd={:?}", full_cmd);
        if (std::system(full_cmd.c_str()) != 0) {
            throw std::runtime_error(fmt::format("Failed to compile {}: command={:?}", cmd->file(), full_cmd));
        }

        // On success, update this file() entry in the directory-level signature TOML.
        sig_map[cmd->output()] = make_signature(*cmd, hash_history);

        toml_dump(sig_map, sig_path);
    }

    return true;
}
