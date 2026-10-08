module;

#include <string>
#include <vector>
#include <unordered_map>

module carton;

static std::unordered_map<std::string, Fingerprint> &fingerprint_of(
    std::unordered_map<std::string, std::unordered_map<std::string, Fingerprint>> &self, const std::string &build_dir
) {
    if (auto it = self.find(build_dir); it != self.end())
        return it->second;

    return self[build_dir] = Fingerprint::parse(build_dir);
}

void CompileCommand::compile_multi(
    std::unordered_map<std::string, std::unordered_map<std::string, Fingerprint>> &fingerprint_map,
    const std::vector<CompileCommand>                                             &commands,
    bool                                                                           precompile
) {
    std::vector<size_t> indices;
    for (size_t i = 0; i < commands.size(); ++i) {
        auto &cc = commands[i];
        if (cc.is_precompile == precompile && !cc.is_done)
            indices.push_back(i);
    }

    fmt::println("[DEBUG] compiling {} items", indices.size());

    if (indices.empty())
        return;

    std::string current_title;
    for (size_t i = 0; i < indices.size(); ++i) {
        auto &cc = commands[indices[i]];

        if (current_title != cc.title) {
            if (!current_title.empty())
                print_end_progress();

            current_title = cc.title;
            print_status(precompile ? "Precompiling" : "Compiling", cc.title);
        }
        print_progress("Building", i, indices.size());

        cc.compile();

        auto &fingerprints      = fingerprint_of(fingerprint_map, cc.directory);
        fingerprints[cc.output] = cc.fp;
    }

    print_progress("Building", indices.size(), indices.size());
    print_end_progress();
}
