module;

#include <reproc++/run.hpp>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <map>
#include <unordered_set>
#include <fstream>
#include <algorithm>

module carton;
import :p1689;
import std.fs;
import cpx.yy_json;

namespace {
    struct Module {
        std::string              name;
        std::vector<std::string> imports;
        std::string              path;
    };
} // namespace

static Module parse_module_p1689(const fs::path &working_dir, const std::string &file, const std::string &cc) {
    std::string     working_dir_str = working_dir.string();
    reproc::options opt;
    opt.redirect.out.type = reproc::redirect::pipe;
    opt.redirect.err.type = reproc::redirect::pipe;
    opt.working_directory = working_dir_str.c_str();

    std::string json;
    std::string errmsg;
    auto [status, ec] = reproc::run(
        std::vector<std::string>{"sh", "-c", f("clang-scan-deps -format=p1689 -- {}", cc)},
        opt,
        reproc::sink::string(json),
        reproc::sink::string(errmsg)
    );

    if (status != 0 || ec) {
        fmt::println(stderr, "{}", errmsg);
        throw ferr("clang-scan-deps failed: file={:?} cc={:?}: {}", file, cc, errmsg);
    }

    // fmt::println(stderr, "{}", json);
    auto p = cpx::yy_json::parse<p1689>(json);
    // fmt::println(stderr, "{}:{}", p.version, p.revision);
    return {.name = p.name(), .imports = p.deps(), .path = file};
}

static Module parse_module(const fs::path &working_dir, const std::string &file, bool strict = true) {
    std::ifstream in(working_dir / file);
    if (!in)
        throw ferr("Failed to open: {}", file);

    std::string              line;
    std::string              name;
    std::vector<std::string> imports;

    auto trim = [](std::string &s) {
        const char *ws = " \t\r\n";
        s.erase(0, s.find_first_not_of(ws));
        s.erase(s.find_last_not_of(ws) + 1);
    };

    while (std::getline(in, line)) {
        trim(line);

        // --- module declaration ---
        if (name.empty()) {
            std::string prefix;

            if (line.starts_with("export module "))
                prefix = "export module ";
            else if (line.starts_with("module "))
                prefix = "module ";

            if (!prefix.empty()) {
                auto rest = line.substr(prefix.size());
                auto pos  = rest.find(';');
                if (pos != std::string::npos) {
                    name = rest.substr(0, pos);
                    trim(name);
                }
                continue;
            }
        }

        // --- import (with optional export) ---
        std::string import_line = line;

        if (import_line.starts_with("export import ")) {
            import_line = import_line.substr(std::string_view("export import ").length());
        } else if (import_line.starts_with("import ")) {
            import_line = import_line.substr(std::string_view("import ").length());
        } else {
            continue;
        }

        auto pos = import_line.find(';');
        if (pos != std::string::npos) {
            auto dep = import_line.substr(0, pos);
            trim(dep);

            if (dep.front() == ':') {
                std::string parent = name;

                auto pos = parent.find(':');
                if (pos != std::string::npos) {
                    parent = parent.substr(0, pos);
                }

                auto d = parent + dep;
                dep    = std::move(d);
            }

            // ignore header units like: import <vector>;
            if (!dep.empty() && dep.front() != '<')
                imports.push_back(dep);
        }
    }

    if (strict && name.empty())
        throw ferr("No module declaration in: {}", file);

    return Module{.name = name, .imports = imports, .path = file};
}

// --- Build graph ---
using Graph = std::unordered_map<std::string, std::vector<std::string>>;

static Graph build_graph(const std::vector<Module> &modules) {
    Graph g;
    for (const auto &m : modules) {
        g[m.name] = m.imports;
    }
    return g;
}

// --- Topological sort ---
static void
dfs(const std::string               &node,
    const Graph                     &g,
    std::unordered_set<std::string> &visiting,
    std::unordered_set<std::string> &visited,
    std::vector<std::string>        &result) {

    if (visited.count(node))
        return;

    if (visiting.count(node))
        throw ferr("Cycle detected at module: {}", node);

    visiting.insert(node);

    if (auto it = g.find(node); it != g.end()) {
        for (const auto &dep : it->second) {
            if (g.count(dep)) { // only consider known modules
                dfs(dep, g, visiting, visited, result);
            }
        }
    }

    visiting.erase(node);
    visited.insert(node);
    result.push_back(node);
}

static std::vector<std::string> topo_sort(const Graph &g) {
    std::vector<std::string>        result;
    std::unordered_set<std::string> visited, visiting;

    for (const auto &[node, _] : g) {
        if (!visited.count(node)) {
            dfs(node, g, visiting, visited, result);
        }
    }

    return result;
}

std::vector<std::string> sort_modules_p1689(
    const std::string                               &working_dir,
    std::vector<std::string>                        &files,
    std::vector<std::string>                        &ccs,
    std::map<std::string, std::vector<std::string>> &mods
) {
    std::vector<Module> modules;
    modules.reserve(files.size());

    for (size_t i = 0; i < files.size(); ++i) {
        auto mod       = parse_module_p1689(working_dir, files[i], ccs[i]);
        mods[mod.name] = mod.imports;
        modules.push_back(mod);
    }

    auto graph = build_graph(modules);
    auto order = topo_sort(graph);

    std::unordered_map<std::string, Module> map;
    for (auto &m : modules)
        map[m.name] = m;

    std::vector<Module> sorted;
    sorted.reserve(order.size());
    for (const auto &name : order) {
        sorted.push_back(map.at(name));
    }

    std::unordered_map<std::string, size_t> rank;
    rank.reserve(order.size());
    for (size_t i = 0; i < order.size(); ++i) {
        rank[sorted[i].path] = i;
    }

    std::sort(files.begin(), files.end(), [&](const std::string &a, const std::string &b) { return rank.at(a) < rank.at(b); });

    return order;
}

std::vector<std::string> sort_modules(
    const std::string &working_dir, std::vector<std::string> &files, std::map<std::string, std::vector<std::string>> &mods
) {
    std::vector<Module> modules;
    modules.reserve(files.size());

    for (auto &file : files) {
        auto mod       = parse_module(working_dir, file);
        mods[mod.name] = mod.imports;
        modules.push_back(mod);
    }

    auto graph = build_graph(modules);
    auto order = topo_sort(graph);

    std::unordered_map<std::string, Module> map;
    for (auto &m : modules)
        map[m.name] = m;

    std::vector<Module> sorted;
    sorted.reserve(order.size());
    for (const auto &name : order) {
        sorted.push_back(map.at(name));
    }

    std::unordered_map<std::string, size_t> rank;
    rank.reserve(order.size());
    for (size_t i = 0; i < order.size(); ++i) {
        rank[sorted[i].path] = i;
    }

    std::sort(files.begin(), files.end(), [&](const std::string &a, const std::string &b) { return rank.at(a) < rank.at(b); });

    return order;
}

std::vector<std::string> collect_module_deps(const std::string &working_dir, const std::string &source) {
    auto mod = parse_module(working_dir, source, false);
    push_unique(mod.imports, mod.name);
    return std::move(mod.imports);
}
