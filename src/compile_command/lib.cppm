module;

#include <string>
#include <vector>

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

    std::string              depfile;
    std::vector<std::string> modnames;
    std::string              title;
    bool                     is_done       = false;
    bool                     is_precompile = false;
    bool                     is_ar         = false;

    void        compile() const;
    static void compile_multi(const std::vector<CompileCommand> &commands, bool precompile = false);
};
