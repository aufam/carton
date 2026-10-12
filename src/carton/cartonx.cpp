module;

#include <spdlog/spdlog.h>
#include <chrono>

module carton;
import cpx;
import cpx.yy_json;

constexpr auto toml_version = cpx::toruniina_toml::spec::v(1, 1, 0);

static int printhelp() {
    fmt::println(
        "cartonx: build and run a single C++ file\n"
        "\n"
        "Usage:\n"
        "  cartonx <FILE> [ARGS]...\n"
        "  cartonx <COMMAND> <FILE>...\n"
        "\n"
        "Commands:\n"
        "  compile-commands <FILE>  Generate compile_commands.json for a source file\n"
        "  clean <FILE>             Remove cached build artifacts for a source file\n"
        "\n"
        "Arguments:\n"
        "  <FILE>                   C++ source file to build and run\n"
        "  [ARGS]...                Arguments passed to the compiled executable\n"
        "\n"
        "Options:\n"
        "  -h, --help               Print this help message\n"
        "\n"
        "Configuration:\n"
        "  Configuration can be specified in the leading comments of the source file\n"
        "  using TOML syntax. Supported sections include [package], [lib],\n"
        "  [dependencies], and [profiles].\n"
        "\n"
        "Examples:\n"
        "  cartonx main.cpp\n"
        "  cartonx main.cpp --verbose\n"
        "  cartonx compile-commands main.cpp\n"
        "  cartonx clean main.cpp\n"
    );
    return 0;
}

// returns parsed first comment paragraph and whether it has shebang
static std::pair<std::string, bool> parse_comment_and_shebang(const fs::path &file_path) {
    std::ifstream file(file_path);
    std::string   comment;
    std::string   line;
    bool          started     = false;
    bool          has_shebang = false;
    bool          first_line  = true;

    while (std::getline(file, line)) {
        if (first_line) {
            first_line  = false;
            has_shebang = line.starts_with("#!");
            if (has_shebang)
                continue;
        }

        if (!started && line.find_first_not_of(" \t\r") == std::string::npos)
            continue;

        if (line.starts_with("// ")) {
            started = true;
            comment += line.substr(3) + '\n';
        } else if (line.starts_with("//")) {
            started = true;
            comment += '\n';
        } else {
            break;
        }
    }

    return {comment, has_shebang};
}

static void copy_without_shebang(const fs::path &src, const fs::path &dest_dir) {
    std::ifstream in(src);
    if (!in)
        throw std::runtime_error("Failed to open source file: " + src.string());

    fs::create_directories(dest_dir);

    std::ofstream out(dest_dir / src.filename());
    if (!out)
        throw std::runtime_error("Failed to open destination file");

    std::string line;

    // Skip the shebang (first line)
    std::getline(in, line);

    // Copy the remaining lines
    while (std::getline(in, line))
        out << line << '\n';
}

int Carton::Cartonx(int argc, char **argv) {
    spdlog::set_level(spdlog::level::warn);
    const auto cache_dir = std::getenv("HOME") + std::string("/.carton");

    if (argc < 2)
        return printhelp();

    const std::string_view arg1 = argv[1];
    if (arg1 == "version")
        return Version();

    if (arg1 == "--help" || arg1 == "-h" || arg1 == "help")
        return printhelp();

    const bool with_compile_commands = arg1 == "compile-commands" || arg1 == "cc";
    const bool do_clean              = arg1 == "clean";
    if ((with_compile_commands || do_clean) && argc < 3)
        return printhelp();

    const fs::path file = fs::absolute(with_compile_commands ? argv[2] : argv[1]);
    if (!fs::is_regular_file(file)) {
        spdlog::error("{:?} is not a regular file", file.string());
        return 1;
    }
    const auto [comment, has_shebang] = parse_comment_and_shebang(file);

    // default values
    Carton ctx;
    ctx.root  = &ctx;
    ctx.cache = std::make_shared<Cache>();

    ctx.cache->directory = cache_dir;

    ctx.package.name    = file.filename().string();
    ctx.package.edition = 23;

    ctx.lib.path = file.parent_path().string();
    ctx.lib.src  = {""};
    ctx.lib.inc  = {""};

    // parse registry
    const auto registry_path = fs::path(cache_dir) / "registry.toml";
    try {
        cpx::toruniina_toml::parse_from_file(registry_path.string(), ctx.registry, toml_version);
    } catch (std::exception &e) {
        spdlog::error("Failed to parse carton packages: {}", e.what());
        exit(1);
    }

    // parse global config
    if (const auto config_path = fs::path(cache_dir) / "config.toml"; fs::exists(config_path))
        try {
            auto cfg     = cpx::toruniina_toml::parse_from_file<Config>(config_path.string(), toml_version);
            ctx.profiles = std::move(cfg.profiles);
        } catch (std::exception &e) {
            spdlog::error("Failed to parse carton config: {}", e.what());
            exit(1);
        }

    // parse comment
    std::tuple parser = {
        cpx::field_ref(ctx.package)      = "package,skipmissing",
        cpx::field_ref(ctx.registry)     = "registry,skipmissing",
        cpx::field_ref(ctx.profiles)     = "profiles,skipmissing",
        cpx::field_ref(ctx.lib)          = "lib,skipmissing",
        cpx::field_ref(ctx.dependencies) = "dependencies,skipmissing",
    };
    try {
        cpx::toruniina_toml::parse(comment, parser, toml_version);
    } catch (std::exception &e) {
        spdlog::warn(e.what());
    }

    // check module support
    ctx.profiles.check_module_support();

    // setup flags
    const auto &profile        = ctx.profiles.dev;
    const auto  sanitizer_flag = profile.sanitize.empty() ? "" : f("-fsanitize={}", fmt::join(profile.sanitize, ","));
    auto       &cache          = *ctx.cache;

    cache.common_flags =
        f( //
            "{} -O{} {} {} {} {}",
            profile.debug ? "-g" : "-DNDEBUG",
            profile.opt_level,
            profile.lto ? "-flto" : "",
            sanitizer_flag,
            profile.arch.empty() ? "" : "-march=" + profile.arch,
            fmt::join(profile.flags, " ")
        );
    cache.common_link_flags =
        f( //
            "{} {} {} {}",
            profile.debug ? "-g" : "",
            profile.lto ? "-flto" : "",
            sanitizer_flag,
            fmt::join(profile.link_flags, " ")
        );

    cache.cppm_standard = std::max(20, ctx.package.edition);

    const auto start = std::chrono::steady_clock::now();

    // collect lib targets
    std::vector<Target *> targets;
    try {
        auto ts = ctx.configure_package(profile, fs::current_path().string());
        push_unique(targets, ts);
    } catch (std::exception &e) {
        spdlog::error("Failed to configure: {}", e.what());
        return 1;
    }

    if (do_clean) {
        targets.front()->clean(profile, cache);
        return 0;
    }

    // collect bin targets
    auto &bin = ctx.bins.emplace_back();
    bin.name  = "main";
    try {
        if (has_shebang && !with_compile_commands) {
            const auto &t         = *targets.front();
            const auto  build_dir = fs::path(cache.directory) / "build" / profile.name / t.output_dir;
            const auto  dest      = build_dir / "main.cpp";
            copy_without_shebang(file, build_dir / "main.cpp");
            bin.path = dest.string();
        } else {
            bin.path = ctx.package.name;
        }
        auto ts = ctx.configure_bins(profile);
        push_unique(targets, ts);
    } catch (std::exception &e) {
        spdlog::error("Failed to configure binaries: {}", e.what());
        return 1;
    }

    if (with_compile_commands) {
        auto &ccs = cache.compile_commands;
        auto  of  = std::ofstream("./compile_commands.json");
        of << cpx::yy_json::dump(ccs, cpx::yy_json::write_flag::pretty_two_spaces);
        return 0;
    }

    // compile and update fingerprint
    auto &fp_map    = cache.fingerprint_map;
    auto  fp_update = [&]() {
        for (auto &[dir, fp] : fp_map) {
            Fingerprint::dump(fp, dir);
        }
    };

    try {
        CompileCommand::compile_multi(fp_map, cache.compile_commands, true);
    } catch (std::exception &e) {
        fp_update();
        spdlog::error("Failed to compile modules: {}", e.what());
        return 1;
    }

    try {
        CompileCommand::compile_multi(fp_map, cache.compile_commands, false);
    } catch (std::exception &e) {
        fp_update();
        spdlog::error("Failed to compile: {}", e.what());
        return 1;
    }

    std::string profile_info = profile.opt_level == 0 ? "unoptimized" : "optimized";
    if (profile.debug)
        profile_info += " + debuginfo";
    if (!profile.sanitize.empty())
        profile_info += f(" + sanitize({})", fmt::join(profile.sanitize, ","));

    std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
    print_status("Finished", f("`{}` profile [{}] target(s) in {:.2f}", profile.name, profile_info, elapsed.count()));
    fp_update();

    // run
    Cli::Run run;
    for (int i = 2; i < argc; i++)
        run.args.emplace_back(argv[i]);

    try {
        return ctx.run(run);
    } catch (std::exception &e) {
        spdlog::error("Failed to run: {}", e.what());
        return 1;
    }
}
