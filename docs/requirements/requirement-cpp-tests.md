**file**: docs/requirements/requirement-cpp-tests.md
**Status**: Active
**Updated**: 2026-10-06
**Philosophy**: CIAO v2.10.2 / CIAO-Lite (Caution • Intentional • Anti-fragile • Over-engineered / Over-protect)

## 1. Purpose

This file owns how gemini-wilgat is checked. WebKit and GTK objects are not split into tiny unit tests. The named checks are full-program checks.

### 1.1 Human-facing

**In one sentence:** You prove a change by building the real program and, when a TP id is implemented, by running that check against the real window.

| Box | Meaning | Example |
|-----|---------|---------|
| You / this login | The person who verifies | You run `./build.sh build` |
| The other role | The test-plan map | It lists TP ids that are still todo |
| Not this file | The product rules those ids check | The owning requirement |

| Includes | Excludes |
|----------|----------|
| Allowed check styles and the TP ids | A requirement to mock WebKit |

| Surface | What you open | What for |
|---------|---------------|----------|
| `./build.sh` | Compile check | The binary links |
| `docs/reviews/test-plan.md` | TP map | Status of each id |

| You do… | What it means | What you type |
|---------|---------------|---------------|
| Smoke-check the tree | The current sources compile | `./build.sh build` |

## 2. Core rules

Allowed:

1. `./build.sh build` compiles every `src/main/cpp/*.cpp` file.
2. A manual run with `DEBUG=1`, `--bookmark`, or `--shutdown`.
3. The TP ids below, when an executable check is added. Until then their status is todo.

Forbidden without an explicit request: isolated unit tests of `auto_follow.cpp`, the login detector, or UI callbacks, and test-only code inside `build.sh`.

| TP-ID | Owner | Pass |
|-------|-------|------|
| TP-CONFIG-CREATE | requirement-cpp-configuration | First launch creates `~/.app/gemini-wilgat/config.conf` |
| TP-COOKIE-PERSIST | requirement-cpp-cookie-management | Cookie file remains after restart and `DEBUG=1` prints a count |
| TP-UA-RESTORE | requirement-cpp-ui-menu | Menu user-agent is applied on the next launch |
| TP-UI-MENU | requirement-cpp-ui-menu | File shows AutoClick, Bookmark, User Agent, Lua Script, Log, and Exit; Lua Script sits before Log; Log sits before Exit; About shows Show Version; User Agent has five items |
| TP-LOG-COMMANDS | requirement-cpp-log-commands | With `HOME` set to an empty temporary directory, `log folder` prints `$HOME/.app/gemini-wilgat/log` and does not create it. `log list` prints nothing and exits 0. After two regular `*.log` files and one non-log file, `log list` numbers the log files in basename order, `log show 1` prints that file unchanged, including enclosure characters, `log show 9` exits 1, and `log clear` deletes only the `*.log` files. `--json` prints one object for list, show, folder, and clear. `log` alone exits 0. The process does not open a window |
| TP-UI-VIEW-APP-LOG | requirement-cpp-log-commands | The HTML for one stored line and one other line is one table. Header cells are Time, Pid, Level, Source, and Message. Odd rows are `#ffffff`, even rows are `#e7eef6`, and hover is `#ffe08a`. No cell text contains `[` `]` `<` `>` `(` `)` `{` `}`. A stored message `Milk low (oat)` shows `Milk low oat`. Source drops a trailing `.cpp` or `.c`, so `@output.cpp` shows `output`. `log show` of that file still prints the brackets and the parentheses. The menu loads that HTML into the main web view |
| TP-LOG-FORMAT | requirement-cpp-log-format | `logcheck --file` passes a stored-line file and fails a `WARN` line, a plain line, and an empty file. `--home` and `--prefix` name two different dated files and do not create them. A missing stamp and a bad stamp exit 1. `--json` prints one object. Help lists `logcheck` under Tests and does not list `test`, `list`, or `show`. The process does not open a window. Go on a passing local `.log` path uses the same HTML function as the menu |
| TP-WEBUI-BOOKMARK | requirement-cpp-webui-bookmark | Bookmark has one item, Start Page, loading `https://gemini.google.com`. No flag loads that URL. `--bookmark=1` warns and still loads Start Page |
| TP-USERSCRIPT-LOAD | requirement-cpp-window | A `*.lua` userscript loads and its URL hook runs |
| TP-CPP-DEBUG-VALUES | requirement-cpp-debug-mode | `DEBUG=1` prints userscript `enabled`, `state`, `path`, and each script's `priority` and `match` |
| TP-CPP-LUA-SOL2 | requirement-cpp-lua-script | The host includes `<sol/sol.hpp>`, loads scripts through `sol::state`, `log` works, and `os` and `io` stay unavailable |
| TP-LUA-SCRIPT-COMMANDS | requirement-cpp-lua-script-commands | With `HOME` set to an empty temporary directory, `luascript folder` prints `$HOME/.app/gemini-wilgat/userscripts` and does not create that directory or `config.conf`. `luascript list` prints nothing and exits 0. After two regular `*.lua` files and one non-lua file, `luascript list` numbers the lua files in basename order, `luascript view 1` prints that file, and `luascript view 9` exits 1. `luascript new gemini-note.lua` creates that file and the directory. A second `new` of the same name exits 1. `luascript new` with standard input not a terminal exits 1 and does not read. `luascript edit` from a pipe replaces the file. `luascript edit` on a terminal exits 1 and does not read a body. `--json` prints one object for folder, list, view, edit, and new. `luascript` alone exits 0. The process does not open a window |
| TP-CPP-DEPENDENCIES | requirement-cpp-dependencies | `pkg-config --exists` succeeds for GTK, WebKitGTK 4.1, GLib, GObject, GIO, SQLite, and Lua 5.3, and both builds request that set |
| TP-WEBUI-AUTO-FOLLOW | requirement-cpp-webui-auto-follow | The allow-list is only `NULL`, so a URI change does not inject. The script clicks `button[aria-label="Follow"]` once and retries the search at 500 ms for at most 20 attempts |
| TP-CPP-LOGGER | requirement-cpp-logger | `test --home /tmp --log-datestamp 20261003` writes one stored line into `/tmp/.app/gemini-wilgat/log/gemini-wilgat-20261003.log`. `test --prefix /tmp` with the same stamp writes a different file under `/tmp/var/gemini-wilgat/log/`. The component on a stored line is the base name of the source file that called `ciao_*` |
| TP-CPP-LOGGER-FOLDER | requirement-cpp-logger-folder | A normal user resolves `$HOME/.app/gemini-wilgat/log`. Root or sudo resolves `/var/gemini-wilgat/log`. `--prefix /tmp` and `--home /tmp` do not share a directory |

## 3. Protection rule

Future editors must not mark a TP id done in `docs/reviews/test-plan.md` until the check has been run.

## 4. Related artifacts

| Artifact | Role |
|----------|------|
| `docs/requirements/index.md` | Registry |
| `docs/reviews/test-plan.md` | TP status |
| `build.sh` | Compile check |

**Last updated**: 2026-10-06
**Owner**: Tests
