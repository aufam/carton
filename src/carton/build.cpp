module carton;

extern "C++" const char *carton_version();
extern "C++" const char *carton_commit();
extern "C++" const char *carton_commit_date();

int Carton::Version() {
    fmt::println("carton {} ({} {})", carton_version(), carton_commit(), carton_commit_date());
    return 0;
}
