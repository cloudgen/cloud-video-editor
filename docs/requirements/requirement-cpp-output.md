**file**: docs/requirements/requirement-cpp-output.md
**Status**: Active
**Updated**: 2026-10-04
**Philosophy**: CIAO v2.10.2 / CIAO-Lite (Caution • Intentional • Anti-fragile • Over-engineered / Over-protect)

## 1. Purpose

All console text from gemini-wilgat goes through `ciao_info`, `ciao_warn`, `ciao_error`, and `ciao_debug`. This file owns those four calls and the quiet and JSON modes. The calls are macros so the dated line can name the source file. That stored line is `requirement-cpp-logger.md`. The directory of that file is `requirement-cpp-logger-folder.md`. Both of those rows are Deferred.

### 1.1 Human-facing

**In one sentence:** You read `[INFO]` lines on the terminal, and `--json` switches that stream to one JSON object per line with human text turned off.

| Box | Meaning | Example |
|-----|---------|---------|
| You / this login | The person watching the terminal | You pass `--quiet` |
| The other role | `output.cpp` | It prints or suppresses each line |
| Not this file | The GTK window, the web page, and the dated log file | The window stays on screen. The file is `requirement-cpp-logger.md` |

| Includes | Excludes |
|----------|----------|
| Levels, quiet, JSON, and the rule that JSON forces quiet | The flag list (`requirement-cpp-parameters.md`) and the dated log file |

| Surface | What you open | What for |
|---------|---------------|----------|
| The terminal | stdout and stderr | These messages |
| `src/main/cpp/output.cpp` | Implementation | `ciao_log` |

| You do… | What it means | What you type |
|---------|---------------|---------------|
| See debug lines | Only `DEBUG=1` enables `ciao_debug` | `DEBUG=1` plus the binary |
| Take a JSON log | Quiet is forced on | `--json` |

## 2. Core rules

- Product code calls `ciao_*` for log lines. `printf`, `fprintf`, `g_print`, and `perror` for those lines are allowed inside `output.cpp` only.
- The `log` verb result is not a `ciao_*` line. `log_commands.cpp` prints it with `g_print` and `g_printerr`. Quiet and JSON mode do not hide or rewrite that result. The verb is `requirement-cpp-log-commands.md`. Help text in `output.cpp` also uses `g_print`.
- The `luascript` verb result is not a `ciao_*` line. `lua_script_commands.cpp` prints it with `g_print` and `g_printerr`. Quiet and JSON mode do not hide or rewrite that result. The verb is `requirement-cpp-lua-script-commands.md`.
- `ciao_error` stays visible in quiet mode. Quiet hides info and debug. Warn stays visible.
- `DEBUG=1` enables `ciao_debug`.
- JSON mode prints one object per line and forces quiet, including when quiet was set another way.

```json
{"timestamp":1746780000,"level":"INFO","message":"WebView initialized"}
```

Levels are `INFO`, `WARN`, `ERROR`, and `DEBUG`.

### 2.1 Project log content types

A project log content type is one kind of message the dated log stores. The five types are the catalog. A message that matches a type is stored. A message that matches none is not required in the dated file. This file still owns the terminal line. The dated file is `requirement-cpp-logger.md`.

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

Quiet and JSON hide terminal lines under the rules above. They do not drop a matching line from the dated file.

## 3. Protection rule

Future editors must not add a second logging API or let JSON mode print a human-readable line beside the object.

## 4. Related artifacts

| Artifact | Role |
|----------|------|
| `docs/requirements/index.md` | Registry |
| `docs/requirements/requirement-cpp-parameters.md` | How quiet, JSON, and DEBUG are turned on |
| `docs/requirements/requirement-cpp-debug-mode.md` | Which C++ userscript values `ciao_debug` must show |
| `src/main/cpp/output.cpp` | Implementation |
| `docs/requirements/requirement-cpp-logger.md` | Dated file, Deferred |
| `docs/requirements/requirement-cpp-logger-folder.md` | Log directory, Deferred |
| `docs/requirements/requirement-cpp-log-commands.md` | `log` verb result, which is not a `ciao_*` line |
| `docs/requirements/requirement-cpp-lua-script-commands.md` | `luascript` verb result, which is not a `ciao_*` line |

**Last updated**: 2026-10-04
**Owner**: Output
