# :package: Carton
C/C++ package manager and build system

## Installation

```bash
# linux
curl -L \
  https://github.com/aufam/carton/releases/latest/download/carton-linux-amd64 \
  -o carton

# macOS
curl -L \
  https://github.com/aufam/carton/releases/latest/download/carton-macos-arm64 \
  -o carton

chmod +x carton
sudo mv carton /usr/local/bin/

# download the latest registry.toml
carton update
```


## Getting Started
Create a new empty project:
```bash
mkdir my-project
cd my-project
carton init my-project
```

Initial `carton.toml` will be:
```toml
#:schema https://raw.githubusercontent.com/aufam/carton/main/carton-schema.json

[package]
edition = 17
name = "my-project"

[dependencies]
```

The project tree will be:
```
.
├── src
│  └── main.cpp
├── .clang-format
├── .gitignore
└── carton.toml
```

Add a dependency (see [carton.io](https://aufam.github.io/carton) for all available packages):
```bash
carton add dotenv
carton add fmt
```

The `carton.toml` will become:
```toml
#:schema https://raw.githubusercontent.com/aufam/carton/main/carton-schema.json

[package]
edition = 17
name = "my-project"

[dependencies]
fmt = { version = "12.2.0" }
dotenv = { version = "1.0" }
```

Configure, build and run:
```
carton
carton build
carton run
```

All artifacts will be stored in:
- Build artifacts: `~/.carton/build/{dev,release}`
- Downloaded sources: `~/.carton/src`
- Registry: `~/.carton/registry.toml`

The only generated artifacts in root project directory is `./compile_commands.json` by running `carton` or its transitive (`carton build` and `carton run`)

You can define global configuration file in `~/.carton/config.toml`, the defaults are:
```toml
#:schema https://raw.githubusercontent.com/aufam/carton/main/config-schema.json

[profile.dev]
cxx = "c++"
c = "cc"
debug = true
lto = false
opt-level = 0
flags = ["-fPIC", "-Wall", "-Wextra"]
sanitize = ["address", "undefined"]

[profile.release]
cxx = "c++"
c = "cc"
debug = false
lto = true
opt-level = 3
flags = ["-fPIC", "-Wall", "-Wextra"]
sanitize = []
```

## Dependencies
Supported compilers:
- Clang
- GCC

Installed system wide:
- git
- curl
- tar

## C++ Module
C++ module is only supported if the compiler is clang. 

`~/.carton/config.toml`:
```toml
#:schema https://raw.githubusercontent.com/aufam/carton/main/config-schema.json

[profile.dev]
cxx = "clang++"
c = "clang"

[profile.release]
cxx = "clang++"
c = "clang"
```


Project structure could be:
```
.
├── src
│  ├── bar
│  │  ├── bar_impl.cpp
│  │  └── lib.cppm
│  ├── foo
│  │  ├── foo_impl.cpp
│  │  ├── foo_other_impl.cpp
│  │  └── lib.cppm
│  ├── lib.cppm
│  └── main.cpp
└── carton.toml
```

