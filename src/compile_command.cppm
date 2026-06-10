module;

#include <cpx/reflect.h>
#include <string>
#include <vector>
#include <unordered_map>

export module carton:compile_command;

export struct CompileCommand {
    std::string file;
    std::string directory;
    std::string command;
    std::string output;
    std::string depfile;

    static bool compile_multi(
        const std::string                            &name,
        const std::vector<CompileCommand>            &commands,
        std::unordered_map<std::string, std::string> &hash_history,
        bool                                          precompile = false
    );
};

template <>
struct cpx::Reflect<CompileCommand> //
    : Fields<
          Reflect<CompileCommand>,
          &CompileCommand::file,
          &CompileCommand::directory,
          &CompileCommand::command,
          &CompileCommand::output> {
    static constexpr TagInfo file      = "file";
    static constexpr TagInfo directory = "directory";
    static constexpr TagInfo command   = "command";
    static constexpr TagInfo output    = "output";

    static constexpr tags_type tags() {
        return std::tie(file, directory, command, output);
    }
};
