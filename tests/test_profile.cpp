module;

#include <gtest/gtest.h>
#include <string>
#include <vector>

module carton;

TEST(ProfileTest, DefaultConstructedProfile) {
    Profile p;
    EXPECT_EQ(p.name, "");
    EXPECT_EQ(p.cxx, "c++");
    EXPECT_EQ(p.c, "cc");
    EXPECT_EQ(p.ar, "ar");
    EXPECT_EQ(p.arch, "native");
    EXPECT_EQ(p.modules, "auto");
    EXPECT_FALSE(p.debug);
    EXPECT_FALSE(p.lto);
    EXPECT_EQ(p.opt_level, 0);
    EXPECT_TRUE(p.sanitize.empty());
    EXPECT_TRUE(p.flags.empty());
    EXPECT_TRUE(p.link_flags.empty());
    EXPECT_FALSE(p._module_support);
}

TEST(ProfileTest, DevProfile) {
    Profile dev = Profile::Dev();
    EXPECT_EQ(dev.name, "dev");
    EXPECT_TRUE(dev.debug);
    EXPECT_FALSE(dev.lto);
    EXPECT_EQ(dev.opt_level, 0);

    std::vector<std::string> expected_sanitize = {"address", "undefined"};
    EXPECT_EQ(dev.sanitize, expected_sanitize);

    std::vector<std::string> expected_flags = {"-fPIC", "-Wall", "-Wextra"};
    EXPECT_EQ(dev.flags, expected_flags);
}

TEST(ProfileTest, ReleaseProfile) {
    Profile rel = Profile::Release();
    EXPECT_EQ(rel.name, "release");
    EXPECT_FALSE(rel.debug);
    EXPECT_TRUE(rel.lto);
    EXPECT_EQ(rel.opt_level, 3);
    EXPECT_TRUE(rel.sanitize.empty());

    std::vector<std::string> expected_flags = {"-fPIC", "-Wall", "-Wextra"};
    EXPECT_EQ(rel.flags, expected_flags);
}

TEST(ProfileTest, ProfilesStruct) {
    Profiles profiles;
    EXPECT_EQ(profiles.dev.name, "dev");
    EXPECT_TRUE(profiles.dev.debug);

    EXPECT_EQ(profiles.release.name, "release");
    EXPECT_FALSE(profiles.release.debug);
    EXPECT_TRUE(profiles.release.lto);
    EXPECT_EQ(profiles.release.opt_level, 3);
}
