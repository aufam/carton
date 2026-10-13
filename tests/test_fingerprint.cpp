module;

#include <gtest/gtest.h>
#include <string>
#include <unordered_map>
#include <filesystem>

module carton;

TEST(FingerprintTest, CompareIdentical) {
    Fingerprint a;
    a.cmd  = "clang++ -O2";
    a.file = "src/main.cpp";
    a.deps = "src/main.cpp: include/header.h";
    a.mods = "carton";

    Fingerprint b = a;
    EXPECT_TRUE(a.compare(b));
    EXPECT_TRUE(b.compare(a));
}

TEST(FingerprintTest, CompareDifferentFields) {
    Fingerprint base;
    base.cmd  = "clang++ -O2";
    base.file = "src/main.cpp";
    base.deps = "deps.d";
    base.mods = "mod";

    Fingerprint diff_cmd = base;
    diff_cmd.cmd         = "clang++ -O3";
    EXPECT_FALSE(base.compare(diff_cmd));

    Fingerprint diff_file = base;
    diff_file.file        = "src/other.cpp";
    EXPECT_FALSE(base.compare(diff_file));

    Fingerprint diff_deps = base;
    diff_deps.deps        = "other_deps.d";
    EXPECT_FALSE(base.compare(diff_deps));

    Fingerprint diff_mods = base;
    diff_mods.mods        = "other_mod";
    EXPECT_FALSE(base.compare(diff_mods));
}

TEST(FingerprintTest, ParseNonExistentDirectory) {
    fs::path temp_dir = fs::path("/tmp") / "carton_test_fp_non_existent";
    fs::remove_all(temp_dir);

    auto result = Fingerprint::parse(temp_dir);
    EXPECT_TRUE(result.empty());
}

TEST(FingerprintTest, DumpAndParseRoundtrip) {
    fs::path temp_dir = fs::path("/tmp") / "carton_test_fp_roundtrip";
    fs::remove_all(temp_dir);
    fs::create_directories(temp_dir);

    std::unordered_map<std::string, Fingerprint> fps;
    Fingerprint                                  fp1;
    fp1.cmd      = "clang++ -c foo.cpp";
    fp1.file     = "foo.cpp";
    fp1.deps     = "foo.o: foo.cpp";
    fp1.mods     = "module_foo";
    fps["foo.o"] = fp1;

    Fingerprint fp2;
    fp2.cmd      = "clang++ -c bar.cpp";
    fp2.file     = "bar.cpp";
    fps["bar.o"] = fp2;

    Fingerprint::dump(fps, temp_dir);

    EXPECT_TRUE(fs::exists(temp_dir / "fingerprints.toml"));

    auto parsed = Fingerprint::parse(temp_dir);
    EXPECT_EQ(parsed.size(), 2);
    EXPECT_TRUE(parsed.contains("foo.o"));
    EXPECT_TRUE(parsed.contains("bar.o"));

    EXPECT_TRUE(parsed["foo.o"].compare(fp1));
    EXPECT_TRUE(parsed["bar.o"].compare(fp2));

    fs::remove_all(temp_dir);
}
