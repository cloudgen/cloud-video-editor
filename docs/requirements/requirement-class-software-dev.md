**file**: docs/requirements/requirement-class-software-dev.md
**Status**: Active
**Area**: class
**Key**: requirement-class-software-dev
**Updated**: 2026-10-04
**Philosophy**: CIAO v2.10.2 / CIAO-Lite (Caution • Intentional • Anti-fragile • Over-engineered / Over-protect)

## 1. Purpose

This workspace is a software product people install and run: a native GTK window. This file is the class law and the residual home for stack facts that no peer requirement owns. The coding-style file is the specialize-in home for portable learned lessons. Without that file, those lessons arrive raw.

### 1.1 Human-facing

**In one sentence:** You run gemini-wilgat on this Linux login; the program opens a Gemini page in a GTK window; this file only records what kind of software project this folder is.

| Box | Meaning | Example |
|-----|---------|---------|
| You / this login | The person who builds or launches the window | `./build.sh build` then run the binary |
| The other role | The window itself | It shows Gemini and stores cookies under `~/.app/gemini-wilgat/` |
| Not this file | Feature behavior, build commands, and coding rules | Those live in the peer requirements |

| Includes | Excludes |
|----------|----------|
| Language, compilers, and which build tool is primary | Bookmark URLs, cookie schema, and menu layout |
| The decision that there is no approval desk | A second class file for another kind of work |

| Surface | What you open | What for |
|---------|---------------|----------|
| `src/main/cpp/project.h` | Runtime name and start page | Machine identity |
| `docs/requirements/index.md` | Registry | Which requirement owns a topic |

| You do… | What it means | What you type |
|---------|---------------|---------------|
| Build the window | The primary path is the simple C++ build | `./build.sh build` |
| Read the coding rules | Portable lessons are specialized in the C++ coding-style requirement | open `requirement-cpp-coding-style.md` |

## 2. Core rules

1. This workspace is software-development. The only class file is `requirement-class-software-dev.md`.
2. This file holds stack facts until a peer owns them. Peers are not copied here.
3. The primary language is C++. Lua is the secondary language of the ship unit.
4. The toolchain class is system `g++`. Version policy is unconstrained: a recent toolchain that passes the build. C++ is C++17. Cross-compilation is out of scope.
5. The primary build tool is `./build.sh` (`install`, `clean`, `build`). Meson is a second build file and must compile the same sources. The dependency set is owned by `requirement-cpp-dependencies.md`. There is no lockfile.
6. The end-user runtime is a Linux session with GTK 3 and WebKitGTK 4.1. The developer machine needs the same libraries plus a C++ compiler and Lua 5.3 headers. Package names are owned by `requirement-cpp-dependencies.md`.
7. Actor / role / subject: **considered — no dest approver and no approval subject**. There is no approval desk.
8. Dest fence conditions: **considered — no dest fence conditions**. Do not invent one.
9. Coding-style related requirement: `requirement-cpp-coding-style.md`. This class file points at it and does not restate the C++ rules.
10. The ship unit does not call `sudo`. `./build.sh install` may run `sudo apt` to install host packages. That is developer bootstrap, not an in-application sudo grant. There is no sudo-command requirement.

### 2.1 Implementation notes (this project)

- Display name: `Gemini (wilgat)` (`PROJECT_NAME` in `src/main/cpp/project.h`).
- Runtime id: `gemini-wilgat` (`PROJECT_ID`).
- Start page: `https://gemini.google.com` (`START_PAGE`). Owned in detail by `requirement-cpp-window.md`.
- Version macros in `project.h` are the runtime version. `project.ini` `VERSION` is the build-script version and may differ until those two files are aligned.
- This folder is not a git repository. No origin URL is recorded.
- Host packages and link modules are owned by `requirement-cpp-dependencies.md`.

## 3. Residual ownership

| Topic | Owner |
|-------|--------|
| Class, languages, compilers, primary build tool | this file |
| C/C++ coding rules | `requirement-cpp-coding-style.md` |
| Window, WebKit order, userscripts, audio environment | `requirement-cpp-window.md` |
| UI menu bar | `requirement-cpp-ui-menu.md` |
| WebUI auto-follow | `requirement-cpp-webui-auto-follow.md` |
| WebUI bookmark list | `requirement-cpp-webui-bookmark.md` |
| `config.conf` | `requirement-cpp-configuration.md` |
| SQLite settings | `requirement-cpp-database.md` |
| Cookies | `requirement-cpp-cookie-management.md` |
| Console output | `requirement-cpp-output.md` |
| C++ userscript debug values | `requirement-cpp-debug-mode.md` |
| C++ Lua host (sol2) | `requirement-cpp-lua-script.md` |
| Lua script menu and `luascript` verb | `requirement-cpp-lua-script-commands.md` |
| C/C++ link and apt dependencies | `requirement-cpp-dependencies.md` |
| Flags and environment variables | `requirement-cpp-parameters.md` |
| `./build.sh` binary path | `requirement-cpp-skills-installation-basic.md` |
| Meson source list | `requirement-cpp-skills-installation-meson.md` |
| Milestone list | `requirement-cpp-milestones.md` |

## 4. Protection rule

Future editors must not add a second class file, invent an approver, invent a dest fence, or move the C++ coding rules out of `requirement-cpp-coding-style.md`.

## 5. Related artifacts

| Artifact | Role |
|----------|------|
| `docs/requirements/index.md` | Registry |
| `docs/requirements/requirement-cpp-coding-style.md` | Coding-style home |
| `docs/requirements/requirement-cpp-window.md` | Live window |
| `src/main/cpp/project.h` | Runtime identity |
| `docs/requirements/requirement-cpp-dependencies.md` | Libraries and apt packages |

**Last updated**: 2026-10-04
**Owner**: Project class
