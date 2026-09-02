module;

#include <string>
#include <vector>
#include <unordered_map>
#include <map>

export module carton:compile_command;
import cpx;

export struct CompileCommand {
    std::string file;
    std::string directory;
    std::string command;
    std::string output;

    static constexpr std::tuple __field_tags__{
        cpx::field<&CompileCommand::file>      = "file      , omitempty",
        cpx::field<&CompileCommand::directory> = "directory , omitempty",
        cpx::field<&CompileCommand::command>   = "command   , omitempty",
        cpx::field<&CompileCommand::output>    = "output    , omitempty",
    };

    void compile() const;

    std::string              depfile;
    std::vector<std::string> modnames;
    bool                     done = false;

    static bool compile_multi(
        const std::string                            &name,
        const std::vector<CompileCommand>            &commands,
        const std::map<std::string, std::string>     &mod_names,
        std::unordered_map<std::string, std::string> &hash_history,
        bool                                          precompile = false
    );
};
