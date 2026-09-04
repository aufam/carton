module;

#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <numeric>
#include <reproc++/run.hpp>

module carton;
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

std::vector<std::string>
sort_modules_p1689(const std::string &working_dir, std::vector<std::string> &files, std::vector<std::string> &ccs) {
    std::vector<Module> modules;
    modules.reserve(files.size());

    for (size_t i = 0; i < files.size(); ++i)
        modules.push_back(parse_module_p1689(working_dir, files[i], ccs[i]));

    auto graph = build_graph(modules);
    auto order = topo_sort(graph);

    std::unordered_map<std::string, size_t> rank;
    rank.reserve(order.size());
    for (size_t i = 0; i < order.size(); ++i)
        rank.emplace(order[i], i);

    std::vector<size_t> indices(modules.size());
    std::iota(indices.begin(), indices.end(), 0);

    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        return rank.at(modules[a].name) < rank.at(modules[b].name);
    });

    // Reorder files and ccs together.
    std::vector<std::string> sorted_files;
    std::vector<std::string> sorted_ccs;

    sorted_files.reserve(files.size());
    sorted_ccs.reserve(ccs.size());

    for (auto i : indices) {
        sorted_files.push_back(std::move(files[i]));
        sorted_ccs.push_back(std::move(ccs[i]));
    }

    files = std::move(sorted_files);
    ccs   = std::move(sorted_ccs);

    return order;
}
