# carton

A C++ package manager and build system — inspired by Cargo, designed for C and C++ projects.

## Overview

`carton` resolves, downloads, and builds C/C++ dependencies by reading a `carton.toml` manifest file. It supports dependencies from:

- A **package registry** (`carton-packages.toml`) via version string
- **Git** repositories (with a specific tag or branch)
- **URLs** pointing to tarballs (`.tar`, `.tar.gz`, `.tar.xz`, `.tar.bz2`, `.tgz`)
- **Local paths**

It performs incremental builds using a hash-based signature system that tracks command changes and header dependencies, so only modified files are recompiled.

## Building carton

**Requirements:** CMake ≥ 3.14, Ninja, a C++17-capable compiler, `git`, `curl`, and `tar`.

```sh
# Debug build
cmake --preset debug
cmake --build build

# Release build (statically linked)
cmake --preset release
cmake --build release
```

The binary is placed in `build/carton` or `release/carton` respectively.

## Usage

Run `carton` from the root of a project that contains a `carton.toml` file:

```sh
carton [OPTIONS]
```

### Options

| Option | Description |
|---|---|
| `--cache <path>` | Directory for cached sources and build artifacts (default: `~/.carton`, env: `CARTON_CACHE`) |
| `--no-default-features` | Disable the `default` feature set |
| `--log-level <level>` | Log verbosity: `trace`, `debug`, `info`, `warn`, `err`, `critical`, `off` (default: `warn`) |

## `carton.toml` Reference

Every project requires a `carton.toml` manifest in its root directory.

```toml
#:schema ./carton-schema.json

[package]
name    = "my-app"
version = "1.0.0"
edition = 17          # C++ standard: 11 | 14 | 17 | 20 | 23 | 26

[lib]
src = ["src/*.cpp"]
inc = ["public:include"]

[dependencies]
fmt    = "11.2.0"
spdlog = { version = "1.15.3", default-features = false }
mylib  = { path = "../mylib" }
cpx    = { git = "https://github.com/aufam/cpx", tag = "v0.1.0" }
zlib   = { url = "https://zlib.net/zlib-1.3.1.tar.gz" }
```

### `[package]`

| Field | Type | Description |
|---|---|---|
| `name` | string | **Required.** Package name |
| `version` | string | **Required.** Semver version (`x.y.z`) |
| `edition` | integer | C++ standard edition (default: `17`) |
| `authors` | array | List of author strings |
| `description` | string | Short description |
| `license` | string | License identifier |

### `[lib]` — library / source target

Describes the sources and headers that make up this package when it is used as a dependency. For top-level projects without a `[lib]` section, `src/` and `include/` are used automatically if they exist.

| Field | Type | Description |
|---|---|---|
| `src` | array | Glob patterns for source files (`.c`, `.cpp`, `.cc`, `.cxx`, `.cppm`) |
| `inc` | array | Include directories. Prefix with `public:` to expose to dependents |
| `lib` | array | Additional static/shared library files to link |
| `flags` | array | Compiler flags. Prefix with `public:` to expose to dependents |
| `link-flags` | array | Linker flags |
| `pre` | string | Shell command to run before compilation (e.g., code generation) |

### `[dependencies]`

Each dependency can be declared as a version string or an inline table:

```toml
[dependencies]
# From the package registry (carton-packages.toml) by version
fmt = "11.2.0"

# Git repository
cpx = { git = "https://github.com/aufam/cpx", tag = "v0.1.0" }

# Local path
mylib = { path = "../mylib" }

# URL to a tarball
zlib = { url = "https://zlib.net/zlib-1.3.1.tar.gz" }
```

Dependency table fields:

| Field | Type | Description |
|---|---|---|
| `version` | string | Version to fetch from the registry |
| `git` | string | Git URL (`https://`, `git@`, or `user/repo` shorthand) |
| `tag` | string | Git tag (mutually exclusive with `branch`) |
| `branch` | string | Git branch (mutually exclusive with `tag`) |
| `url` | string | URL of a downloadable archive |
| `path` | string | Local filesystem path |
| `subdir` | string | Subdirectory inside the resolved path |
| `features` | array | Features to enable |
| `default-features` | boolean | Whether to include the dependency's default features (default: `true`) |
| `optional` | boolean | Only include when explicitly requested by a feature (default: `false`) |
| `src` | array | Override source files |
| `inc` | array | Override include directories |
| `lib` | array | Additional library files |
| `flags` | array | Additional compiler flags |
| `link-flags` | array | Additional linker flags |
| `pre` | string | Pre-build shell command |

### `[features]`

The features system lets you define optional capability groups, similar to Cargo features:

```toml
[features]
default    = ["fmt"]
fmt        = []
fmt-external = ["dep:fmt", "dep:fmt-external-flag"]
```

- List feature names in `default` to enable them unless `--no-default-features` is passed.
- `dep:<name>` activates the named optional dependency.
- A `nodefault` feature is activated when `--no-default-features` is used.

### `[vars]`

Define variables that are interpolated into any string field using `{var}` syntax. A default value can be supplied with `{var:default}`.

```toml
[vars]
fmt-version = "11.2.0"

[dependencies]
spdlog = { version = "1.15.3" }
fmt    = "{fmt-version}"
```

### `[profiles]`

Override compiler and flags for the `release` and `debug` profiles:

```toml
[profiles.release]
id    = "release"
cxx   = "clang++"
c     = "clang"
flags = ["-O3", "-DNDEBUG", "-fPIC"]

[profiles.debug]
id    = "debug"
cxx   = "clang++"
c     = "clang"
flags = ["-g", "-fPIC"]
link-flags = ["-fsanitize=address"]
```

## Package Registry (`carton-packages.toml`)

The registry file maps package names to their source and build metadata. Version strings in dependencies are resolved against this file. Variable substitution with `{version}` (the requested version) is supported in all fields.

```toml
[fmt.package]
name    = "fmt"
edition = 11

[fmt.lib]
git = "https://github.com/fmtlib/fmt"
tag = "{version}"
src = ["src/format.cc", "src/os.cc"]
```

The registry ships with entries for commonly used C++ libraries including `fmt`, `spdlog`, `yyjson`, `toml11`, `nlohmann_json`, `rapidjson`, `CLI11`, `Boost.PFR`, `magic_enum`, `catch2`, `gtest`, `ftxui`, `cpptrace`, `httplib`, and more.

## How it Works

1. **Parse** `carton-packages.toml` and `carton.toml`.
2. **Resolve** each dependency: clone from git, download an archive, or use a local path.
3. **Compile** dependency sources (release profile) into object files under `~/.carton/build/release/`.
4. **Compile** the project's own sources (debug profile) into `~/.carton/build/debug/`.
5. **Link** everything into `~/.carton/bin/<name>`.

Rebuilds are skipped for any file whose compiler command, output hash, and header dependency hashes are unchanged since the last run.

## License

MIT — see `carton.toml` for details.
