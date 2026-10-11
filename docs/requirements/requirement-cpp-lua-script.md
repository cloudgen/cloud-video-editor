**file**: docs/requirements/requirement-cpp-lua-script.md
**Status**: Active
**Updated**: 2026-10-04
**Philosophy**: CIAO v2.10.2 / CIAO-Lite (Caution • Intentional • Anti-fragile • Over-engineered / Over-protect)

## 1. Purpose

The C++ userscript host is sol2. This file owns that binding: one `sol::state`, the libraries it opens, and how a `*.lua` file is run. Which scripts run, and what the four Lua functions do, stays in `requirement-cpp-window.md`. Debug field names stay in `requirement-cpp-debug-mode.md`.

### 1.1 Human-facing

**In one sentence:** You place a `.lua` file in the userscripts directory, and `userscript_manager.cpp` runs it through one sol2 state.

| Box | Meaning | Example |
|-----|---------|---------|
| You / this login | The person who adds a page hook | You write `gemini-log.lua` |
| The other role | `userscript_manager.cpp` | It holds one `sol::state` |
| Not this file | Directory, `@match`, `@priority`, the four function behaviors, and the menu and `luascript` verb | `requirement-cpp-window.md` and `requirement-cpp-lua-script-commands.md` |

| Includes | Excludes |
|----------|----------|
| sol2 as the only Lua binding, the opened libraries, and per-script error isolation | A second Lua VM, and the debug field list |

| Surface | What you open | What for |
|---------|---------------|----------|
| `src/main/cpp/userscript_manager.cpp` | The host | sol2 state and script load |
| `src/main/cpp/userscript_manager.h` | C linkage boundary | The two functions the other units call |

| You do… | What it means | What you type |
|---------|---------------|---------------|
| Run a script on the sol2 state | The file is loaded with `script_file` after the state exists | place `gemini-log.lua` in the userscripts directory |

## 2. Core rules

- The Lua binding is sol2. The include is `<sol/sol.hpp>`. Types live in namespace `sol`. Product code does not call the Lua C API (`lua_State`, `luaL_*`, `lua_*`).
- There is one `sol::state`, a member of `LuaScriptManager`. Other translation units do not see that state. They call `initialize_userscript_manager()` and `userscript_on_url_changed()`. The `extern "C"` guards on those two functions stay as written in `requirement-cpp-coding-style.md`.
- Libraries opened, and no others: `sol::lib::base`, `sol::lib::string`, `sol::lib::table`, `sol::lib::math`. `os` and `io` stay closed.
- The host registers functions with `sol::state::set_function`. The names are `log`, `inject_javascript`, `get_current_url`, and `register_on_url_changed`. What each one does is owned by `requirement-cpp-window.md`. The host adds no other Lua global.
- URL hooks are stored as `sol::function`. Each call is wrapped so a `sol::error` from one hook does not skip the remaining hooks.
- Each `*.lua` file is loaded with `sol::state::script_file`. A `sol::error` from one file is logged and the next file still loads.
- Creating the state catches `sol::error`. On that failure the manager stays disabled and does not scan the directory.
- sol2 headers are the tree `./build.sh install` places so `<sol/sol.hpp>` resolves. The clone step stays in `requirement-cpp-skills-installation-basic.md`. C++17 stays in `requirement-class-software-dev.md`.

### 2.1 Implementation notes (this project)

Host: `src/main/cpp/userscript_manager.cpp`. The state is constructed inside `LuaScriptManager` after the WebKit view is accepted and scripts are enabled.

```cpp
#include <sol/sol.hpp>

sol::state lua;
lua.open_libraries(sol::lib::base, sol::lib::string, sol::lib::table, sol::lib::math);
lua.set_function("log", [](const std::string& msg) { /* ciao_info */ });
lua.script_file(path);  // sol::error is per file
```

`register_on_url_changed` receives a `sol::function`. The process keeps one `LuaScriptManager` (`g_script_manager`). A second `initialize_userscript_manager()` call does not create a second state.

### 2.2 Why this requirement exists

- **CIAO Principle 5** (https://github.com/cloudgen/ciao): one binding. Lua enters the process only through sol2.
- **CIAO Principle 1**: a script that throws is reported as `sol::error` and the other scripts still load.

## 3. Design principles

- **Caution**: `os` and `io` are not opened. The sol2 state is not handed to C.
- **Intentional**: sol2 is the named library. A new Lua global is added to the name list in this file in the same change as the `set_function` call.
- **Anti-fragile**: one bad file or one bad hook is a `sol::error` for that file or hook alone.
- **Over-protect**: the Lua C API is not a second way into the same state.

## 4. Verification

| TP-ID | Pass |
|-------|------|
| TP-CPP-LUA-SOL2 | `userscript_manager.cpp` includes `<sol/sol.hpp>` and loads scripts through `sol::state`. A loaded script can call `log`. `os` and `io` are unavailable |

Map: `docs/reviews/test-plan.md`. Status stays todo until that check is run.

## 5. Protection rule

Future editors must not add a second Lua state, open `sol::lib::os` or `sol::lib::io`, call the Lua C API from product code, or move the `@match` and `@priority` rules out of `requirement-cpp-window.md`.

## 6. Related artifacts

| Artifact | Role |
|----------|------|
| `docs/requirements/index.md` | Registry |
| `docs/requirements/requirement-cpp-window.md` | Which scripts run, and the four functions |
| `docs/requirements/requirement-cpp-lua-script-commands.md` | Menu and `luascript` verb |
| `docs/requirements/requirement-cpp-debug-mode.md` | Debug field list |
| `docs/requirements/requirement-cpp-coding-style.md` | `extern "C"` on the two entry points |
| `docs/requirements/requirement-cpp-skills-installation-basic.md` | sol2 headers on the simple build |
| `docs/requirements/requirement-cpp-dependencies.md` | sol2 headers as a compile dependency |
| `docs/requirements/requirement-cpp-skills-installation-meson.md` | `/usr/local/include` for the same header |
| `docs/reviews/test-plan.md` | TP-CPP-LUA-SOL2 |
| `src/main/cpp/userscript_manager.cpp` | The sol2 host |
| `src/main/cpp/userscript_manager.h` | C boundary |

**Last updated**: 2026-10-04
**Owner**: C++ Lua script
