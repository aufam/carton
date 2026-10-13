module carton;

extern "C++" const char *carton_version();

int Carton::Version() {
    fmt::println("carton version {}", carton_version());
    return 0;
}
