module;

#include <spdlog/spdlog.h>

module carton;
import cpx.toruniina_toml;

constexpr auto toml_version = cpx::toruniina_toml::spec::v(1, 1, 0);

Carton Carton::New(const std::string &cache_dir) {
    Carton ctx;
    ctx.root  = &ctx;
    ctx.cache = std::make_shared<Cache>();

    ctx.cache->directory = cache_dir;

    // parse registry
    const auto registry_path = [&]() {
        fs::path ret = ".carton/registry.toml";
        if (!fs::exists(ret))
            ret = fs::path(cache_dir) / "registry.toml";
        return ret;
    }();

    try {
        cpx::toruniina_toml::parse_from_file(registry_path.string(), ctx.registry, toml_version);
    } catch (std::exception &e) {
        spdlog::error("Failed to parse carton packages: {}", e.what());
        exit(1);
    }

    // parse config
    const auto config_path = [&]() {
        fs::path ret = ".carton/config.toml";
        if (!fs::exists(ret))
            ret = fs::path(cache_dir) / "config.toml";
        return ret;
    }();

    if (fs::exists(config_path))
        try {
            auto cfg     = cpx::toruniina_toml::parse_from_file<Config>(config_path.string(), toml_version);
            ctx.profiles = std::move(cfg.profiles);
        } catch (std::exception &e) {
            spdlog::error("Failed to parse carton config: {}", e.what());
            exit(1);
        }

    // parse context
    try {
        cpx::toruniina_toml::parse_from_file("./carton.toml", ctx, toml_version);
    } catch (std::exception &e) {
        spdlog::error("Failed to parse carton package: {}", e.what());
        exit(1);
    }

    // validate
    ctx.profiles.check_module_support();

    return ctx;
}
