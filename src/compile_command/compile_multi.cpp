module;

#include <string>
#include <vector>

module carton;

void CompileCommand::compile_multi(const std::vector<CompileCommand> &commands, bool precompile) {
    std::vector<size_t> indices;
    for (size_t i = 0; i < commands.size(); ++i) {
        auto &cc = commands[i];
        if (cc.is_precompile == precompile && !cc.is_done)
            indices.push_back(i);
    }

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
    }

    print_progress("Building", indices.size(), indices.size());
    print_end_progress();
}
