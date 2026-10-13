module;

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>

module carton;

TEST(CartonTest, Version) {
    EXPECT_EQ(Carton::Version(), 0);
}

TEST(CartonTest, InitInEmptyDirectory) {
    fs::path temp_dir = fs::path("/tmp") / "carton_test_init_dir";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    const auto prev_cwd = fs::current_path();
    fs::current_path(temp_dir);

    Package pkg;
    pkg.name    = "test_project";
    pkg.version = "0.1.0";
    pkg.edition = 20;

    int ret = Carton::Init(pkg);
    EXPECT_EQ(ret, 0);

    EXPECT_TRUE(fs::exists("carton.toml"));
    EXPECT_TRUE(fs::exists("src/main.cpp"));

    // Calling Init again when carton.toml exists must return 1
    int second_ret = Carton::Init(pkg);
    EXPECT_EQ(second_ret, 1);

    fs::current_path(prev_cwd);
    fs::remove_all(temp_dir);
}

TEST(CartonTest, AddDependencyNotInRegistry) {
    fs::path temp_dir = fs::path("/tmp") / "carton_test_add_dir";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    const auto prev_cwd = fs::current_path();
    fs::current_path(temp_dir);

    // Create a dummy carton.toml
    {
        std::ofstream of("carton.toml");
        of << "[package]\nname = \"my_app\"\nversion = \"0.1.0\"\n";
    }

    Carton     ctx;
    Dependency dep;
    dep.name    = "unknown_pkg";
    dep.version = "1.0.0";

    // Unknown dependency not in ctx.registry should return 1
    int res = ctx.add(dep);
    EXPECT_EQ(res, 1);

    fs::current_path(prev_cwd);
    fs::remove_all(temp_dir);
}

TEST(CartonTest, AddDependencySuccess) {
    fs::path temp_dir = fs::path("/tmp") / "carton_test_add_success_dir";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    const auto prev_cwd = fs::current_path();
    fs::current_path(temp_dir);

    {
        std::ofstream of("carton.toml");
        of << "[package]\nname = \"my_app\"\nversion = \"0.1.0\"\n";
    }

    Carton ctx;
    // Add a package to registry
    Carton reg_pkg;
    reg_pkg.package.name     = "fmt";
    reg_pkg.package.versions = {"10.0.0", "9.1.0"};
    ctx.registry.emplace("fmt", std::move(reg_pkg));

    Dependency dep;
    dep.name    = "fmt";
    dep.version = "10.0.0";

    int res = ctx.add(dep);
    EXPECT_EQ(res, 0);

    // Check that carton.toml now contains the dependency
    std::ifstream ifs("carton.toml");
    std::string   content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    EXPECT_NE(content.find("[dependencies]"), std::string::npos);
    EXPECT_NE(content.find("fmt"), std::string::npos);
    EXPECT_NE(content.find("10.0.0"), std::string::npos);

    // Adding already existing dependency should return 1
    ctx.dependencies["fmt"] = dep;
    EXPECT_EQ(ctx.add(dep), 1);

    fs::current_path(prev_cwd);
    fs::remove_all(temp_dir);
}
