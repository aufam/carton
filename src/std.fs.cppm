module;

#include <filesystem>

export module std.fs;

export namespace fs {
    using ::std::filesystem::path;

    using ::std::filesystem::copy;
    using ::std::filesystem::copy_file;
    using ::std::filesystem::create_directories;
    using ::std::filesystem::current_path;
    using ::std::filesystem::exists;
    using ::std::filesystem::is_directory;
    using ::std::filesystem::read_symlink;
    using ::std::filesystem::remove;
    using ::std::filesystem::remove_all;
    using ::std::filesystem::rename;
    using ::std::filesystem::space;
} // namespace fs
