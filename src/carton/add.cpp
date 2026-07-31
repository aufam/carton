module;

#include <algorithm>
#include <spdlog/spdlog.h>

module carton;

int Carton::add(Dependency &dep) const {
    const std::string filename = "carton.toml";

    if (dependencies.contains(dep.name)) {
        spdlog::error("{:?} already exists in {}", dep.name, filename);
        return 1;
    }

    auto reg = registry.find(dep.name);
    if (reg == registry.end()) {
        spdlog::error("Cannot find {:?} in the package registry", dep.name);
        return 1;
    }

    auto &versions = reg->second.package.versions;

    if (dep.empty()) {
        if (versions.empty()) {
            spdlog::error("version not specified");
            return 1;
        }
        dep.version = versions.front();
    } else if (!dep.version.empty() && std::find(versions.begin(), versions.end(), dep.version) == versions.end()) {
        spdlog::error("invalid version {:?}, expect one of: {}", dep.version, versions);
        return 1;
    } else if (!dep.git.empty() && dep.branch.empty() && dep.tag.empty() && dep.commit.empty()) {
        spdlog::error("no branch|tag|commit specified");
        return 1;
    }

    std::ifstream ifs(filename);
    if (!ifs) {
        spdlog::error("Cannot open {}", filename);
        return 1;
    }

    std::stringstream buffer;
    buffer << ifs.rdbuf();
    std::string text = buffer.str();

    // Build the dependency line.
    std::ostringstream dep_line;
    dep_line << dep.name << " = {";
    bool first = true;

    auto add_field = [&](std::string_view name, const auto &value) {
        if (!first)
            dep_line << ",";
        dep_line << " " << name << " = " << fmt::format("{:?}", value);
        first = false;
    };

    if (!dep.version.empty())
        add_field("version", dep.version);
    if (!dep.path.empty())
        add_field("path", dep.path);
    if (!dep.url.empty())
        add_field("url", dep.url);
    if (!dep.git.empty())
        add_field("git", dep.git);
    if (!dep.tag.empty())
        add_field("tag", dep.tag);
    if (!dep.branch.empty())
        add_field("branch", dep.branch);
    if (!dep.commit.empty())
        add_field("commit", dep.commit);
    if (!dep.subdir.empty())
        add_field("subdir", dep.subdir);

    if (!dep.features.empty()) {
        if (!first)
            dep_line << ",";
        dep_line << " features = " << fmt::format("{}", dep.features);
        first = false;
    }

    if (dep.optional) {
        if (!first)
            dep_line << ",";
        dep_line << " optional = true";
        first = false;
    }

    if (dep.default_features.has_value()) {
        if (!first)
            dep_line << ",";
        dep_line << " default-features = " << (*dep.default_features ? "true" : "false");
    }

    dep_line << " }\n";

    const std::string header = "[dependencies]";
    auto              pos    = text.find(header);
    if (pos == std::string::npos) {
        // No dependencies table yet.
        if (!text.empty() && text.back() != '\n')
            text += '\n';
        text += "\n[dependencies]\n";
        text += dep_line.str();
    } else {
        // Find the next table.
        auto table_begin = pos + header.size();
        auto next_table  = text.find("\n[", table_begin);

        if (next_table == std::string::npos)
            next_table = text.size();

        // Insert before the next table (i.e. at the end of [dependencies]).
        text.insert(next_table, dep_line.str());
    }

    std::ofstream ofs(filename, std::ios::trunc);
    ofs << text;
    return 0;
}
