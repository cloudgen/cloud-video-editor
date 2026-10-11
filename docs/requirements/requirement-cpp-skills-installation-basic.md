**file**: docs/requirements/requirement-cpp-skills-installation-basic.md
**Status**: Active
**Updated**: 2026-10-04
**Philosophy**: CIAO v2.10.2 / CIAO-Lite (Caution • Intentional • Anti-fragile • Over-engineered / Over-protect)

## 1. Purpose

This file owns the simple GCC build, which is the primary way to compile gemini-wilgat. The binary name comes from `PRJ` in `project.ini`.

### 1.1 Human-facing

**In one sentence:** You run `./build.sh build` and the program appears at `target/exe/debug/VideoTools`.

| Box | Meaning | Example |
|-----|---------|---------|
| You / this login | The person who compiles | You run the three verbs below |
| The other role | `build.sh` | It compiles every `src/main/cpp/*.cpp` file |
| Not this file | Meson | `requirement-cpp-skills-installation-meson.md` |

| Includes | Excludes |
|----------|----------|
| `install`, `clean`, `build` / `simple` | Ant, Make, and Gradle verbs |

| Surface | What you open | What for |
|---------|---------------|----------|
| `./build.sh` | Build script | Compile |
| `target/exe/debug/VideoTools` | Binary | Run |

| You do… | What it means | What you type |
|---------|---------------|---------------|
| Build and run | Dependencies, then compile, then the binary | `./build.sh install && ./build.sh build && ./target/exe/debug/gemini-wilgat` |

## 2. Core rules

`./build.sh` accepts only:

| Verb | Behavior |
|------|----------|
| `install` | Install distro packages. Also ensures sol2 headers when `<sol/sol.hpp>` is missing |
| `deps` | Alias of `install` |
| `clean` | Remove build output |
| `build` | Compile and link |
| `simple` | Alias of `build` |
| empty | Same as `build` |

`build` compiles every `src/main/cpp/*.cpp` with `g++ -std=c++17`. A `src/main/cpp/*.c` file is compiled with `gcc -std=c11`. The compile adds `-I/usr/local/include` for `<sol/sol.hpp>`. The link line is `g++` when any `.cpp` file is present, and `gcc` when the directory contains only `.c` files.

The output path is `${BUILD_DEBUG}/${PRJ}`. With the current `project.ini` that is `target/exe/debug/gemini-wilgat`.

`install` may call `sudo apt` for host packages. The package names are the apt set in `requirement-cpp-dependencies.md`. The gemini-wilgat window does not call `sudo`.

## 3. Protection rule

Future editors must not document `target/exe/debug/r2webpad` as this binary, or document a `./build.sh` verb that the `case` statement does not implement.

## 4. Related artifacts

| Artifact | Role |
|----------|------|
| `docs/requirements/index.md` | Registry |
| `docs/requirements/requirement-cpp-skills-installation-meson.md` | Second build |
| `build.sh` | Implementation |
| `project.ini` | `PRJ=gemini-wilgat` |
| `docs/requirements/requirement-cpp-lua-script.md` | Binding the sol2 headers serve |
| `docs/requirements/requirement-cpp-dependencies.md` | Apt packages and the pkg-config set |

**Last updated**: 2026-10-04
**Owner**: Simple build
