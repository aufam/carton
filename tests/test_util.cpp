module;

#include <gtest/gtest.h>
#include <string>
#include <vector>

module carton;

TEST(UtilTest, PushUniqueString) {
    std::vector<std::string> vec;

    // Empty string is ignored
    push_unique(vec, "");
    EXPECT_TRUE(vec.empty());

    // Single item added
    push_unique(vec, "alpha");
    EXPECT_EQ(vec.size(), 1);
    EXPECT_EQ(vec[0], "alpha");

    // Duplicate item not added
    push_unique(vec, "alpha");
    EXPECT_EQ(vec.size(), 1);

    // New item added to back
    push_unique(vec, "beta");
    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(vec[1], "beta");

    // Item added to front
    push_unique(vec, "first", true);
    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0], "first");
    EXPECT_EQ(vec[1], "alpha");
    EXPECT_EQ(vec[2], "beta");

    // Duplicate item with front=true still not added
    push_unique(vec, "beta", true);
    EXPECT_EQ(vec.size(), 3);
}

TEST(UtilTest, PushUniqueVectorString) {
    std::vector<std::string> vec   = {"existing"};
    std::vector<std::string> items = {"alpha", "existing", "beta"};

    push_unique(vec, items);
    EXPECT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0], "existing");
    EXPECT_EQ(vec[1], "alpha");
    EXPECT_EQ(vec[2], "beta");
}

TEST(UtilTest, PushUniquePointers) {
    int                a = 1, b = 2;
    std::vector<int *> vec;

    // Nullptr is ignored
    push_unique(vec, static_cast<int *>(nullptr));
    EXPECT_TRUE(vec.empty());

    push_unique(vec, &a);
    EXPECT_EQ(vec.size(), 1);
    EXPECT_EQ(vec[0], &a);

    // Duplicate pointer ignored
    push_unique(vec, &a);
    EXPECT_EQ(vec.size(), 1);

    // Add to front
    push_unique(vec, &b, true);
    EXPECT_EQ(vec.size(), 2);
    EXPECT_EQ(vec[0], &b);
    EXPECT_EQ(vec[1], &a);

    // Vector of pointers
    std::vector<int *> batch = {&a, &b, nullptr};
    push_unique(vec, batch);
    EXPECT_EQ(vec.size(), 2);
}

TEST(UtilTest, PushBackUnique) {
    std::vector<std::string> vec = {"first", "second", "third"};

    // Empty string ignored
    push_back_unique(vec, "");
    EXPECT_EQ(vec.size(), 3);

    // New string added to back
    push_back_unique(vec, "fourth");
    EXPECT_EQ(vec.size(), 4);
    EXPECT_EQ(vec.back(), "fourth");

    // Existing string moved from middle to back
    push_back_unique(vec, "second");
    EXPECT_EQ(vec.size(), 4);
    std::vector<std::string> expected = {"first", "third", "fourth", "second"};
    EXPECT_EQ(vec, expected);

    // Vector overload
    std::vector<std::string> more = {"third", "fifth"};
    push_back_unique(vec, more);
    expected = {"first", "fourth", "second", "third", "fifth"};
    EXPECT_EQ(vec, expected);
}

TEST(UtilTest, FormatHelpers) {
    auto formatted = f("value: {}", 42);
    EXPECT_EQ(formatted, "value: 42");

    auto err = ferr("failed with code {}", 500);
    EXPECT_STREQ(err.what(), "failed with code 500");
}

TEST(UtilTest, ExpandPathBasic) {
    std::vector<std::string> sources = {};
    expand_path(".", sources);
    EXPECT_TRUE(sources.empty());

    // Valid existing directory and file
    sources = {"CMakeLists.txt"};
    expand_path(".", sources, true);
    EXPECT_EQ(sources.size(), 1);
    EXPECT_EQ(sources[0], "CMakeLists.txt");

    // Glob expansion
    sources = {"src/*.cppm"};
    expand_path(".", sources, true);
    EXPECT_FALSE(sources.empty());
    for (const auto &s : sources) {
        EXPECT_TRUE(s.ends_with(".cppm"));
    }

    // Non-existent path with check_exist=true throws
    sources = {"this_file_does_not_exist_at_all.xyz"};
    EXPECT_THROW(expand_path(".", sources, true), std::runtime_error);

    // Non-existent path with check_exist=false does not throw
    sources = {"this_file_does_not_exist_at_all.xyz"};
    EXPECT_NO_THROW(expand_path(".", sources, false));
    EXPECT_EQ(sources.size(), 1);
    EXPECT_EQ(sources[0], "this_file_does_not_exist_at_all.xyz");
}

TEST(UtilTest, CollectCppmGlobs) {
    std::vector<std::string> cpps;
    auto                     globs = collect_cppm_globs(".", "src", &cpps);

    // Root src contains lib.cppm, so "src/*.cppm" should be in globs
    bool found_root_globs = false;
    for (const auto &g : globs) {
        if (g == "src/*.cppm") {
            found_root_globs = true;
            break;
        }
    }
    EXPECT_TRUE(found_root_globs);

    // Subdirectories with lib.cppm (like src/target/lib.cppm) should also have globs
    bool found_target_glob = false;
    for (const auto &g : globs) {
        if (g == "src/target/*.cppm") {
            found_target_glob = true;
            break;
        }
    }
    EXPECT_TRUE(found_target_glob);

    // cpps should also have collected entries
    EXPECT_FALSE(cpps.empty());
    bool found_target_cpp = false;
    for (const auto &c : cpps) {
        if (c == "src/target/*") {
            found_target_cpp = true;
            break;
        }
    }
    EXPECT_TRUE(found_target_cpp);
}
