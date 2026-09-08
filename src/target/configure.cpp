module;

#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <xxhash.h>
#include <spdlog/spdlog.h>

module carton;
import cpx.fmt;

static constexpr uint64_t fingerprint_seed = 1469598103934665603ull;

static uint64_t hash64(std::string_view s, XXH3_state_t *state = nullptr) {
    const bool own = !state;

    if (own) {
        state = XXH3_createState();
        XXH3_64bits_reset(state);
    }

    XXH3_64bits_update(state, s.data(), s.size());
    const uint64_t h = XXH3_64bits_digest(state);

    if (own)
        XXH3_freeState(state);

    return h;
}

static uint64_t hash64_combine(uint64_t h, uint64_t v) {
    return h ^ (v + 0x9e3779b97f4a7c15ull + (h << 6) + (h >> 2));
}

static std::vector<std::string> parse_depfile(const fs::path &path) {
    std::ifstream in(path);
    if (!in)
        return {};

    std::vector<std::string> result;
    std::string              line;
    std::string              content;

    while (std::getline(in, line)) {
        const bool continued = !line.empty() && line.back() == '\\';

        if (continued)
            line.pop_back();

        content += line;

        if (continued)
            continue;

        const auto colon = content.find(':');
        if (colon != std::string::npos) {
            std::istringstream iss(content.substr(colon + 1));

            std::string dep;
            while (iss >> dep)
                result.push_back(std::move(dep));
        }

        content.clear();
    }

    return result;
}

static uint64_t hash_file(Cache &self, const std::string &file) {
    // cache hit
    if (auto it = self.hash_history.find(file); it != self.hash_history.end())
        return it->second;

    std::ifstream in(file, std::ios::binary);
    uint64_t      h = 1469598103934665603ull;
    if (!in)
        return h;

    auto state = XXH3_createState();
    XXH3_64bits_reset(state);
    auto _ = cpx::defer([state]() { XXH3_freeState(state); });
    char buffer[4096];
    while (in.read(buffer, sizeof(buffer)) || in.gcount() > 0) {
        std::string_view chunk(buffer, in.gcount());
        h = hash64_combine(h, hash64(chunk, state));
    }

    self.hash_history[file] = h;
    return h;
}

static uint64_t hash_modules(Cache &cache, const std::vector<std::string> &modules) {
    uint64_t h = fingerprint_seed;

    for (const auto &module : modules)
        h = hash64_combine(h, hash_file(cache, cache.mod_paths.at(module)));

    return h;
}

static uint64_t hash_depfiles(Cache &cache, const fs::path &depfile) {
    uint64_t h = fingerprint_seed;

    for (const auto &file : parse_depfile(depfile))
        h = hash64_combine(h, hash_file(cache, file));

    return h;
}

static Fingerprint make_fingerprint(Cache &cache, const CompileCommand &cc) {
    Fingerprint fp;

    fp.cmd = f("{:016x}", hash64(cc.command));
    if (cc.file == "__dummy__.c")
        return fp;

    fp.file = f("{:016x}", hash_file(cache, cc.file));
    fp.mods = f("{:016x}", hash_modules(cache, cc.modnames));
    fp.deps = f("{:016x}", hash_depfiles(cache, cc.depfile));

    return fp;
}

static std::unordered_map<std::string, Fingerprint> &fingerprint_of(Cache &self, const std::string &build_dir) {
    if (auto it = self.fingerprint_map.find(build_dir); it != self.fingerprint_map.end())
        return it->second;

    return self.fingerprint_map[build_dir] = Fingerprint::parse(build_dir);
}

static bool
update_fingerprint(std::unordered_map<std::string, Fingerprint> &fingerprints, CompileCommand &cc, const Fingerprint &fp) {
    auto &cached = fingerprints[cc.output];
    bool  done   = fp.compare(cached);

    if (!done)
        cached = fp;

    return done;
}

static void add_bmi_flags(const Cache &cache, const std::vector<std::string> &modules, std::vector<std::string> &bmi_flags) {
    for (const auto &m : modules) {
        push_unique(bmi_flags, f("-fmodule-file={}='{}'", m, cache.bmi_paths.at(m)));
    }
}

static std::pair<CompileCommand, std::string> make_module_command(
    const Target                   &self,
    const Profile                  &profile,
    const Cache                    &cache,
    const fs::path                 &build_dir,
    const fs::path                 &mod_path,
    const std::string              &mod_name,
    const std::vector<std::string> &bmi_flags
) {
    CompileCommand cc;

    cc.title         = self.title;
    cc.is_precompile = true;
    cc.directory     = build_dir.string();
    cc.file          = (self.working_dir / mod_path).string();
    cc.output        = mod_path.string() + ".o";
    cc.depfile       = mod_path.string() + ".d";
    cc.modnames      = self.transitive_modules;

    auto pcm = f("{}-{}.bmi", cache.cppm_standard, mod_name);
    std::replace(pcm.begin(), pcm.end(), ':', '-');

    cc.command =
        f( //
            "{} {} -std=c++{} -fmacro-prefix-map=\"{}\"=\"{}\" -x c++-module {} {} "
            "-fmodule-output='{}' -o '{}' -c '{}' -MMD -MP -MF '{}'",
            profile.cxx,
            cache.common_flags,
            cache.cppm_standard,
            self.working_dir,
            self.name,
            fmt::join(self.flags, " "),
            fmt::join(bmi_flags, " "),
            pcm,
            cc.output,
            cc.file,
            cc.depfile
        );

    return {std::move(cc), std::move(pcm)};
}

static bool configure_modules(
    Target                                       &self,
    const Profile                                &profile,
    Cache                                        &cache,
    const fs::path                               &build_dir,
    std::vector<std::string>                     &bmi_flags,
    std::vector<std::string>                     &objs,
    std::unordered_map<std::string, Fingerprint> &fingerprints
) {
    bool recompile = false;

    std::vector<std::string> transitive_names;

    for (size_t i = 0; i < self.mod.size(); ++i) {
        const auto &mod_path = self.mod[i];
        const auto &mod_name = self.modules[i];

        auto [cc, pcm] = make_module_command(self, profile, cache, build_dir, mod_path, mod_name, bmi_flags);
        push_unique(cc.modnames, transitive_names);

        cache.mod_paths[mod_name] = cc.file;
        cache.bmi_paths[mod_name] = (build_dir / pcm).string();

        const auto fp = make_fingerprint(cache, cc);

        cc.is_done = update_fingerprint(fingerprints, cc, fp) && fs::exists(build_dir / cc.output);
        recompile  = recompile || !cc.is_done;

        objs.push_back((build_dir / cc.output).string());
        cache.compile_commands.push_back(std::move(cc));

        push_unique(bmi_flags, f("-fmodule-file={}='{}'", mod_name, cache.bmi_paths.at(mod_name)));
        push_unique(transitive_names, mod_name);
    }

    return recompile;
}

static CompileCommand make_source_command(
    const Target                   &self,
    const Profile                  &profile,
    const Cache                    &cache,
    const fs::path                 &build_dir,
    const fs::path                 &entry,
    const std::vector<std::string> &bmi_flags
) {
    CompileCommand cc;

    cc.title     = self.title;
    cc.directory = build_dir.string();
    cc.output    = entry.string() + ".o";
    cc.depfile   = entry.string() + ".d";
    cc.file      = (self.working_dir / entry).string();

    const auto ext = entry.extension();

    if (ext == ".cpp" || ext == ".cxx" || ext == ".cc" || ext == ".mm") {
        cc.command =
            f( //
                "{} {} -std=c++{} -fmacro-prefix-map=\"{}\"=\"{}\" {} {} "
                "-o '{}' -c '{}' -MMD -MP -MF '{}'",
                profile.cxx,
                cache.common_flags,
                self.edition,
                self.working_dir,
                self.name,
                fmt::join(self.flags, " "),
                self.edition < 20 ? "" : f("{}", fmt::join(bmi_flags, " ")),
                cc.output,
                cc.file,
                cc.depfile
            );

        if (self.edition >= 20) {
            push_unique(cc.modnames, self.transitive_modules);
            push_unique(cc.modnames, self.modules);
        }

    } else if (ext == ".c" || ext == ".s" || ext == ".asm" || ext == ".S" || ext == ".m") {
        cc.command =
            f( //
                "{} {} -fmacro-prefix-map=\"{}\"=\"{}\" {} "
                "-o '{}' -c '{}' -MMD -MP -MF '{}'",
                profile.c,
                cache.common_flags,
                self.working_dir,
                self.name,
                fmt::join(self.flags, " "),
                cc.output,
                cc.file,
                cc.depfile
            );
    }

    return cc;
}

static bool configure_sources(
    Target                                       &self,
    const Profile                                &profile,
    Cache                                        &cache,
    const fs::path                               &build_dir,
    const std::vector<std::string>               &bmi_flags,
    std::vector<std::string>                     &objs,
    std::unordered_map<std::string, Fingerprint> &fingerprints
) {
    bool recompile = false;

    for (const auto &entry : self.src) {
        auto cc = make_source_command(self, profile, cache, build_dir, entry, bmi_flags);

        if (cc.command.empty())
            continue;

        const auto fp = make_fingerprint(cache, cc);

        cc.is_done = update_fingerprint(fingerprints, cc, fp) && fs::exists(build_dir / cc.output);
        recompile  = recompile || !cc.is_done;

        objs.push_back((build_dir / cc.output).string());
        cache.compile_commands.push_back(std::move(cc));
    }

    return recompile;
}

static bool configure_output(
    Target                                       &self,
    const Profile                                &profile,
    Cache                                        &cache,
    const fs::path                               &build_dir,
    const std::vector<std::string>               &objs,
    std::string_view                              type,
    bool                                          recompile,
    std::unordered_map<std::string, Fingerprint> &fingerprints,
    bool                                          is_module = false
) {
    CompileCommand cc;

    cc.title     = self.title;
    cc.directory = build_dir.string();
    cc.file      = "__dummy__.c";

    if (type == "exe") {
        cc.output = self.output_name;
        cc.command =
            f( //
                "{} -o \"{}\" {} '{}' {}",
                profile.cxx,
                cc.output,
                cache.common_link_flags,
                fmt::join(objs, "' '"),
                fmt::join(self.link_flags, " ")
            );

        self.executable = (build_dir / cc.output).string();
    } else if (type == "dyn") {
        cc.output = "lib" + self.output_name + ".so";
        cc.command =
            f( //
                "{} -shared -o \"{}\" {} '{}' {}",
                profile.cxx,
                cc.output,
                cache.common_link_flags,
                fmt::join(objs, "' '"),
                fmt::join(self.link_flags, " ")
            );
    } else {
        if (objs.empty())
            return false;

        cc.is_ar  = true;
        cc.output = "lib" + self.output_name;
        if (is_module)
            cc.output += f(".std{}", cache.cppm_standard);
        cc.output += ".a";
        cc.command = f("{} rcs '{}' '{}'", profile.ar, cc.output, fmt::join(objs, "' '"));

        push_unique(self.link_flags, f("{:?}", (build_dir / cc.output).string()), true);
    }

    const auto fp = make_fingerprint(cache, cc);
    cc.is_done    = update_fingerprint(fingerprints, cc, fp) && !recompile && fs::exists(build_dir / cc.output);

    cache.compile_commands.push_back(std::move(cc));
    return !cc.is_done;
}

void Target::configure(const Profile &profile, Cache &cache, std::string_view type) {
    spdlog::trace("target configure before {}", *this);

    const auto build_dir = fs::path(cache.directory) / "build" / profile.name / output_dir;
    fs::create_directories(build_dir);

    auto &fingerprints = fingerprint_of(cache, build_dir);

    std::vector<std::string> objs;
    std::vector<std::string> bmi_flags;
    add_bmi_flags(cache, transitive_modules, bmi_flags);

    bool recompile = false;
    if (profile._module_support && !mod.empty()) {
        std::vector<std::string> objs;
        recompile |= configure_modules(*this, profile, cache, build_dir, bmi_flags, objs, fingerprints);
        recompile |= configure_output(*this, profile, cache, build_dir, objs, "ar", recompile, fingerprints, false);
    }

    recompile |= configure_sources(*this, profile, cache, build_dir, bmi_flags, objs, fingerprints);
    configure_output(*this, profile, cache, build_dir, objs, type, recompile, fingerprints);

    Fingerprint::dump(fingerprints, build_dir);

    spdlog::trace("target configure after {}", *this);
}
