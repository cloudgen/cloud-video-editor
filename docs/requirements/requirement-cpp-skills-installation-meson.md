**file**: docs/requirements/requirement-cpp-skills-installation-meson.md
**Status**: Active
**Updated**: 2026-10-04
**Philosophy**: CIAO v2.10.2 / CIAO-Lite (Caution • Intentional • Anti-fragile • Over-engineered / Over-protect)

## 1. Purpose

This file owns `meson.build`. Meson compiles the same C++ sources as `./build.sh`.

### 1.1 Human-facing

**In one sentence:** You can compile gemini-wilgat with Meson, and that build compiles every `src/main/cpp/*.cpp` file.

| Box | Meaning | Example |
|-----|---------|---------|
| You / this login | The person who prefers Meson | You run `meson setup builddir` |
| The other role | `meson.build` | It names every source file |
| Not this file | The `./build.sh` verbs | `requirement-cpp-skills-installation-basic.md` |

| Includes | Excludes |
|----------|----------|
| Project name, language, `dependency()` call sites, and the source list | The package names themselves (`requirement-cpp-dependencies.md`) |

| Surface | What you open | What for |
|---------|---------------|----------|
| `meson.build` | Build definition | Sources and dependencies |
| `builddir` | Meson output | The `gemini-wilgat` executable |

| You do… | What it means | What you type |
|---------|---------------|---------------|
| Configure and compile | Out-of-source build | `meson setup builddir && meson compile -C builddir` |

## 2. Core rules

- `project()` name is `gemini-wilgat`. The language is C++17.
- The executable name is `gemini-wilgat`.
- `dependency()` entries are the link set in `requirement-cpp-dependencies.md`, each `required: true`, including `threads`.
- C++ include directory: `/usr/local/include` so `sol/sol.hpp` resolves. What that header is for is owned by `requirement-cpp-lua-script.md`.
- Sources are every `src/main/cpp/*.cpp` file.
- `userscript_manager.cpp` is not optional. It is its own translation unit beside `src/main/cpp/main.cpp`.
- Stale names that are not sources: `src/main/cpp/load_page.c`, `src/main/cpp/menu_bookmark_gemini.c`, `src/main/cpp/url_bar.c`. The page loader on disk is `src/main/cpp/load_page_ogin_check.cpp`.

### 2.1 Implementation notes (this project)

Current sources:

- `src/main/cpp/auto_follow.cpp`
- `src/main/cpp/config.cpp`
- `src/main/cpp/cookies.cpp`
- `src/main/cpp/dialog_about.cpp`
- `src/main/cpp/dialog_version.cpp`
- `src/main/cpp/load_page_ogin_check.cpp`
- `src/main/cpp/login_detector_app.cpp`
- `src/main/cpp/log_commands.cpp`
- `src/main/cpp/main.cpp`
- `src/main/cpp/menu_bookmark.cpp`
- `src/main/cpp/menu_bookmark_app.cpp`
- `src/main/cpp/menu_exit.cpp`
- `src/main/cpp/menu_log.cpp`
- `src/main/cpp/menu_user_agent.cpp`
- `src/main/cpp/output.cpp`
- `src/main/cpp/path_utils.cpp`
- `src/main/cpp/settings.cpp`
- `src/main/cpp/ui_menu_app.cpp`
- `src/main/cpp/ui_window_app.cpp`
- `src/main/cpp/url_bar_go_button.cpp`
- `src/main/cpp/url_bar_page.cpp`
- `src/main/cpp/url_bar_user_agent.cpp`
- `src/main/cpp/webkit_setup.cpp`
- `src/main/cpp/userscript_manager.cpp`

The `project()` version is 1.0.7, the same numbers as the `project.h` macros. `project.ini` `VERSION` and the README badge stay 1.0.6 until those two are aligned. Do not invent another version number.

## 3. Protection rule

Future editors must not drop `userscript_manager.cpp` from `meson.build` while `webkit_setup.cpp` calls `initialize_userscript_manager()`.

## 4. Related artifacts

| Artifact | Role |
|----------|------|
| `docs/requirements/index.md` | Registry |
| `docs/requirements/requirement-cpp-skills-installation-basic.md` | Primary build |
| `meson.build` | This build |
| `src/main/cpp/userscript_manager.cpp` | C++ userscript loader |
| `docs/requirements/requirement-cpp-lua-script.md` | sol2 binding those headers provide |
| `docs/requirements/requirement-cpp-dependencies.md` | Link set those `dependency()` calls must match |

**Last updated**: 2026-10-04
**Owner**: Meson build
