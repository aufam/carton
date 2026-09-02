module;

#include <string>
#include <map>
#include <vector>
#include <spdlog/spdlog.h>
#include <reproc++/run.hpp>
#include <xxhash.h>

module carton;

static uint64_t hash64(std::string_view s, XXH3_state_t *state = nullptr) {
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

static uint64_t hash64_combine(uint64_t h, uint64_t v) {
    h ^= v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2);
    return h;
}

void Target::precompile(const Profile &profile, Cache &cache) {
    const fs::path cache_dir = cache.directory;
    const fs::path build_dir = cache_dir / "build" / profile.name / output_dir;
    fs::create_directories(build_dir);

    auto &fingerprints = cache.fingerprint_of(build_dir);

    for (auto &cc : precompile_commands) {
        auto fp = Fingerprint{};
        fp.cmd  = f("{:016x}", hash64(cc.command));
        fp.file = f("{:016x}", cache.hash_file(cc.file));

        uint64_t h = 1469598103934665603ull;
        for (auto &modname : cc.modnames) {
            h = hash64_combine(h, cache.hash_file(cache.mod_paths.at(modname)));
        }
        fp.mods = f("{:016x}", h);

        h = 1469598103934665603ull;
        for (auto file : parse_depfile(build_dir / cc.depfile)) {
            h = hash64_combine(h, cache.hash_file(file));
        }
        fp.deps = f("{:016x}", h);

        auto &fp_cache = fingerprints[cc.file];
        if (!fp.compare(fp_cache)) {
            cc.recompile = true;
            fp_cache     = fp;
        } else {
            cc.recompile = false;
        }
    }

    for (auto &cc : compile_commands) {
        auto fp = Fingerprint{};
        fp.cmd  = f("{:016x}", hash64(cc.command));
        fp.file = f("{:016x}", cache.hash_file(cc.file));

        uint64_t h = 1469598103934665603ull;
        for (auto &modname : cc.modnames) {
            h = hash64_combine(h, cache.hash_file(cache.mod_paths.at(modname)));
        }
        fp.mods = f("{:016x}", h);

        h = 1469598103934665603ull;
        for (auto file : parse_depfile(build_dir / cc.depfile)) {
            h = hash64_combine(h, cache.hash_file(file));
        }
        fp.deps = f("{:016x}", h);

        auto &fp_cache = fingerprints[cc.file];
        if (!fp.compare(fp_cache)) {
            cc.recompile = true;
            fp_cache     = fp;
        } else {
            cc.recompile = false;
        }
    }

    Fingerprint::dump(fingerprints, build_dir);
}

void Target::compile(const Profile &profile, Cache &cache) {
    print_progress("Building", 0, );
}
