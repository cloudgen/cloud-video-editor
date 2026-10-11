**file**: docs/requirements/requirement-cpp-debug-mode.md
**Status**: Active
**Updated**: 2026-10-04
**Philosophy**: CIAO v2.10.2 / CIAO-Lite (Caution • Intentional • Anti-fragile • Over-engineered / Over-protect)

## 1. Purpose

The C++ userscript loader must show its important runtime values when debug mode is on, so a missed script or a skipped hook can be read from the terminal. This file owns that field list. Turning debug on stays in `requirement-cpp-parameters.md`. The print function stays in `requirement-cpp-output.md`.

### 1.1 Human-facing

**In one sentence:** You start gemini-wilgat with `DEBUG=1` and the terminal shows the userscript loader's enabled flag, state, path, each script's priority and match, and the URL that did or did not run a hook.

| Box | Meaning | Example |
|-----|---------|---------|
| You / this login | The person inspecting a script that did not run | You launch with debug on |
| The other role | `userscript_manager.cpp` | It prints one named field line per moment |
| Not this file | How quiet and JSON hide lines | `requirement-cpp-output.md` |

| Includes | Excludes |
|----------|----------|
| The C++ field list and when each line is printed | A second debug flag, and the cookie or settings dumps |

| Surface | What you open | What for |
|---------|---------------|----------|
| The terminal | Debug lines | The values below |
| `src/main/cpp/userscript_manager.cpp` | The loader | Where the lines are printed |

| You do… | What it means | What you type |
|---------|---------------|---------------|
| Inspect the loader | Debug is on and quiet is off, so each important value is printed | `DEBUG=1 ./target/exe/debug/gemini-wilgat` |

## 2. Core rules

- C++ code prints important values only by calling `ciao_debug`. It does not use `std::cout`, `printf`, `fprintf`, or `g_print`.
- Debug mode is the existing `DEBUG=1` switch. There is no C++-only flag.
- Each value is printed as `name=value`. A line that describes an event and omits the value does not satisfy this file.
- Quiet mode and JSON mode follow `requirement-cpp-output.md`. Those modes do not add a human debug line.
- Do not print cookie contents, config secrets, or the body of injected JavaScript. The JavaScript length in bytes is allowed.

### 2.1 Project log content types

A project log content type is one kind of message the dated log stores. The five types are the catalog. A message that matches a type is stored. A message that matches none is not required in the dated file. The field list below is the **debug** type for the userscript loader. **gnome-debug** is a separate type in the same catalog. The stored line is `requirement-cpp-logger.md`.

| Type | What is stored | When it is stored |
|------|----------------|-------------------|
| info | Ordinary progress | The program reports a normal step |
| warning | A recoverable problem | The program continues after the problem |
| error | A failure the operator must see | The step failed |
| debug | A diagnostic detail | Debug mode is on |
| gnome-debug | A GNOME, GTK, or GLib log-domain line | The platform emits it, including when debug mode is off |

`ciao_info` stores **info**. `ciao_warn` stores **warning** with the token `WARNING`. `ciao_error` stores **error**. `ciao_debug` stores **debug**, and only when debug mode is on.

**gnome-debug** domains are `GLib`, `GLib-GObject`, `Gtk`, `Gdk`, and `WebKit`. Levels are `ERROR`, `CRITICAL`, `WARNING`, `MESSAGE`, `INFO`, and `DEBUG`. The line shape is `<Domain>-<LEVEL> **:` plus the message. `g_warning`, `g_critical`, `g_message`, `g_debug`, and `g_log` are this type. A line that appears only on the terminal does not satisfy this type.

Found in this project:

| Where | Example that must be stored |
|-------|-----------------------------|
| `src/main/cpp/url_bar_user_agent.cpp` | `Gtk-CRITICAL` after a broken Enter-key signal on the URL bar |
| `src/main/cpp/menu_bookmark_app.cpp` | `g_warning("[AUTOCLICK] Cannot find valid web_view")` |

### 2.2 Implementation notes (this project)

The loader is `src/main/cpp/userscript_manager.cpp`. Lines use the prefix `[USERSCRIPT]`.

| Moment | Required fields |
|--------|-----------------|
| `init` | `enabled`, `state`, `hooks`, `path`. `reason` when init stops early |
| `script` | `priority`, `match`, `path`, once per `*.lua` file seen |
| `loaded` | `enabled`, `state`, `hooks`, `path` after the scan finishes or fails |
| `url-changed` | `enabled`, `state`, `hooks`, `path`, `url`. `reason` when hooks are not called |
| `inject` | `bytes` only |

`state` is `IDLE`, `SCANNING`, `LOADING`, `LOADED`, or `ERROR`.

`enabled` is `1` or `0`. `hooks` is the number of `register_on_url_changed` functions. `path` is the resolved scripts directory, or `(empty)`. `match` is the `-- @match` text, or `(none)`. `url` is the page URI, or `(null)`.

Early `reason` values: `invalid-web-view`, `disabled`, `empty-path`, `lua-init-error`, `already-initialized`, `no-manager`, `null-uri`, `no-hooks`, `scan-error`.

Sample lines (the path text is whatever the process resolved; this file does not freeze a home directory):

```text
[DEBUG] [USERSCRIPT] init enabled=1 state=IDLE hooks=0 path=<resolved-scripts-directory>
[DEBUG] [USERSCRIPT] script priority=50 match=https://gemini.google.com/* path=<script-file>
[DEBUG] [USERSCRIPT] loaded enabled=1 state=LOADED hooks=1 path=<resolved-scripts-directory>
[DEBUG] [USERSCRIPT] url-changed enabled=1 state=LOADED hooks=1 path=<resolved-scripts-directory> url=https://gemini.google.com/
[DEBUG] [USERSCRIPT] inject bytes=24
```

### 2.3 Why this requirement exists

- **CIAO Principle 5** (https://github.com/cloudgen/ciao): one output path. The values go through `ciao_debug`.
- **CIAO Principle 1**: a skipped hook must show `enabled`, `hooks`, and `url` instead of returning with no trace.

## 3. Design principles

- **Caution**: debug lines exist only when `DEBUG=1` and quiet is off.
- **Intentional**: the field names above are the list. A new important value is added to the table in the same change as the `ciao_debug` call.
- **Anti-fragile**: a script that fails to load still prints its `priority`, `match`, and `path` before the error.
- **Over-protect**: the JavaScript body is not a debug value.

## 4. Verification

| TP-ID | Pass |
|-------|------|
| TP-CPP-DEBUG-VALUES | With `DEBUG=1` and quiet off, startup prints `enabled`, `state`, and `path`, and each loaded `*.lua` file prints `priority` and `match` |

Map: `docs/reviews/test-plan.md`. Status stays todo until that check is run.

## 5. Protection rule

Future editors must not add a second logger for these values, drop `match` or `priority` from the script line, or print the JavaScript body to satisfy this file.

## 6. Related artifacts

| Artifact | Role |
|----------|------|
| `docs/requirements/index.md` | Registry |
| `docs/requirements/requirement-cpp-output.md` | `ciao_debug`, quiet, and JSON |
| `docs/requirements/requirement-cpp-parameters.md` | `DEBUG=1` |
| `docs/requirements/requirement-cpp-window.md` | Userscript behavior |
| `docs/requirements/requirement-cpp-lua-script.md` | sol2 host |
| `docs/reviews/test-plan.md` | TP-CPP-DEBUG-VALUES |
| `src/main/cpp/userscript_manager.cpp` | The lines |

**Last updated**: 2026-10-04
**Owner**: C++ debug mode
