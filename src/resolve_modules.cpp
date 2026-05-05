#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <algorithm>

namespace {
    struct Module {
        std::string              name;
        std::vector<std::string> imports;
        std::string              path;
    };
} // namespace

// TODO: use clang-scan-deps
static Module parse_module(const std::filesystem::path &working_dir, const std::string &file) {
    std::ifstream in(working_dir / file);
    if (!in)
        throw std::runtime_error("Failed to open: " + file);

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

            if (line.rfind("export module ", 0) == 0)
                prefix = "export module ";
            else if (line.rfind("module ", 0) == 0)
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

        if (import_line.rfind("export import ", 0) == 0) {
            import_line = import_line.substr(std::string_view("export import ").length());
        } else if (import_line.rfind("import ", 0) == 0) {
            import_line = import_line.substr(std::string_view("import ").length());
        } else {
            continue;
        }

        auto pos = import_line.find(';');
        if (pos != std::string::npos) {
            auto dep = import_line.substr(0, pos);
            trim(dep);

            // ignore header units like: import <vector>;
            if (!dep.empty() && dep.front() != '<')
                imports.push_back(dep);
        }
    }

    if (name.empty())
        throw std::runtime_error("No module declaration in: " + file);

    return Module{name, imports, file};
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
        throw std::runtime_error("Cycle detected at module: " + node);

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

std::vector<std::string> sort_modules(const std::string &working_dir, std::vector<std::string> &files) {
    std::vector<Module> modules;
    modules.reserve(files.size());

    for (auto &file : files) {
        modules.push_back(parse_module(working_dir, file));
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
