module;

#include <gtest/gtest.h>
#include <string>
#include <vector>

module carton;

TEST(TargetTest, IsInterface) {
    Target t;
    EXPECT_TRUE(t.is_interface());

    t.src = {"src/foo.cpp"};
    EXPECT_FALSE(t.is_interface());
    t.src.clear();

    t.mod = {"src/lib.cppm"};
    EXPECT_FALSE(t.is_interface());
    t.mod.clear();

    t.public_flags = {"-DFOO"};
    EXPECT_FALSE(t.is_interface());
    t.public_flags.clear();

    t.link_flags = {"-lpthread"};
    EXPECT_FALSE(t.is_interface());
    t.link_flags.clear();

    t.link_objects = {"libfoo.a"};
    EXPECT_FALSE(t.is_interface());
    t.link_objects.clear();

    EXPECT_TRUE(t.is_interface());
}

TEST(TargetTest, NewFromDependencyFlagsAndIncludes) {
    Dependency dep;
    dep.path  = ".";
    dep.flags = {"public:-DFEATURE_ENABLED", "-DINTERNAL_ONLY"};
    dep.inc   = {"public:include", "src/internal"};

    auto target = Target::New(dep);
    ASSERT_NE(target, nullptr);
    EXPECT_EQ(target->working_dir, ".");

    // Flags: public flag should be in both flags and public_flags; internal only in flags
    std::vector<std::string> expected_public_flags = {"-DFEATURE_ENABLED", "-I\"include\""};
    EXPECT_EQ(target->public_flags, expected_public_flags);

    bool found_internal = false;
    bool found_feature  = false;
    for (const auto &f : target->flags) {
        if (f == "-DINTERNAL_ONLY")
            found_internal = true;
        if (f == "-DFEATURE_ENABLED")
            found_feature = true;
    }
    EXPECT_TRUE(found_internal);
    EXPECT_TRUE(found_feature);
}

TEST(TargetTest, NewFromDependencySourceFiltering) {
    Dependency dep;
    dep.path = ".";
    // Target::New calls expand_path with check_exist=true, so we use actual existing files in carton repo
    dep.src  = {"src/main.cpp", "CMakeLists.txt", "README.md"};

    auto target = Target::New(dep);
    ASSERT_NE(target, nullptr);

    // main.* should be filtered out
    // CMakeLists.txt and README.md should be filtered out because they are not valid source extensions
    EXPECT_TRUE(target->src.empty());
}

TEST(TargetTest, NewFromDependencyOsPrefix) {
    Dependency dep;
    dep.path       = ".";
    dep.link_flags = {"linux:-lrt", "win:-lws2_32", "osx:-framework Cocoa", "-lm"};

    auto target = Target::New(dep);
    ASSERT_NE(target, nullptr);

#if defined(__linux__)
    // "linux:-lrt" should become "-lrt"
    EXPECT_EQ(target->link_flags.size(), 2ul);
    if (target->link_flags.size() == 2ul) {
        EXPECT_EQ(target->link_flags[0], "-lrt");
        EXPECT_EQ(target->link_flags[1], "-lm");
    }
#elif defined(__APPLE__)
    // "osx:-framework Cocoa" should become "-framework Cocoa"
    EXPECT_EQ(target->link_flags.size(), 2ul);
    if (target->link_flags.size() == 2ul) {
        EXPECT_EQ(target->link_flags[0], "-framework Cocoa");
        EXPECT_EQ(target->link_flags[1], "-lm");
    }
#endif
}

TEST(TargetTest, NewFromBinary) {
    Binary bin;
    bin.name = "my_bin";
    bin.path = "CMakeLists.txt";

    auto target = Target::New(".", bin);
    ASSERT_NE(target, nullptr);
    EXPECT_EQ(target->working_dir, ".");
    EXPECT_EQ(target->src.size(), 1ul);
    EXPECT_EQ(target->src[0], "CMakeLists.txt");
}

TEST(TargetTest, AddDependencyFlagsAndObjects) {
    Target main_target;
    main_target.name    = "app";
    main_target.edition = 20;

    Target dep_target;
    dep_target.name               = "dep";
    dep_target.edition            = 17;
    dep_target.flags              = {"-Iprivate_dir"};
    dep_target.public_flags       = {"-Ipublic_dir"};
    dep_target.link_flags         = {"-lz"};
    dep_target.link_objects       = {"libz.a"};
    dep_target.modules            = {"dep.mod"};
    dep_target.transitive_modules = {"dep.transitive"};

    // Add dependency without extend (default: only public flags inherited)
    main_target.add_dependency(dep_target, false, false);

    EXPECT_EQ(main_target.dependencies.size(), 1ul);
    EXPECT_EQ(main_target.dependencies[0], &dep_target);

    // flags should contain public flags of dependency, but not private flags
    bool has_public  = false;
    bool has_private = false;
    for (const auto &f : main_target.flags) {
        if (f == "-Ipublic_dir")
            has_public = true;
        if (f == "-Iprivate_dir")
            has_private = true;
    }
    EXPECT_TRUE(has_public);
    EXPECT_FALSE(has_private);

    EXPECT_EQ(main_target.link_flags, std::vector<std::string>{"-lz"});
    EXPECT_EQ(main_target.link_objects, std::vector<std::string>{"libz.a"});

    // Transitive modules should include other's transitive_modules and modules
    bool has_transitive = false;
    bool has_mod        = false;
    for (const auto &m : main_target.transitive_modules) {
        if (m == "dep.transitive")
            has_transitive = true;
        if (m == "dep.mod")
            has_mod = true;
    }
    EXPECT_TRUE(has_transitive);
    EXPECT_TRUE(has_mod);

    // Adding the same dependency again is a no-op (idempotent)
    main_target.add_dependency(dep_target, false, false);
    EXPECT_EQ(main_target.dependencies.size(), 1ul);
}

TEST(TargetTest, AddDependencyWithExtendAndPublic) {
    Target main_target;
    main_target.name    = "app";
    main_target.edition = 20;

    Target dep_target;
    dep_target.name         = "dep";
    dep_target.edition      = 20;
    dep_target.flags        = {"-Iprivate_dir"};
    dep_target.public_flags = {"-Ipublic_dir"};

    // With extend=true, all flags are inherited
    // With public_=true, public flags are added to main_target's public_flags
    main_target.add_dependency(dep_target, true, true);

    bool has_private = false;
    for (const auto &f : main_target.flags) {
        if (f == "-Iprivate_dir")
            has_private = true;
    }
    EXPECT_TRUE(has_private);

    bool has_public_in_public = false;
    for (const auto &f : main_target.public_flags) {
        if (f == "-Ipublic_dir")
            has_public_in_public = true;
    }
    EXPECT_TRUE(has_public_in_public);
}

TEST(TargetTest, AddDependenciesVector) {
    Target main_target;
    main_target.edition = 20;

    Target dep1;
    dep1.edition      = 17;
    dep1.public_flags = {"-DDEP1"};

    Target dep2;
    dep2.edition      = 17;
    dep2.public_flags = {"-DDEP2"};

    std::vector<Target *> deps = {&dep1, &dep2};
    main_target.add_dependencies(deps);

    EXPECT_EQ(main_target.dependencies.size(), 2ul);
    EXPECT_EQ(main_target.flags.size(), 2ul);
}
