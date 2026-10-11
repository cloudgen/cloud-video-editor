**file**: docs/requirements/requirement-cpp-logger.md
**Status**: Deferred
**Updated**: 2026-10-05
**Philosophy**: CIAO v2.10.2 / CIAO-Lite (Caution • Intentional • Anti-fragile • Over-engineered / Over-protect)

## 1. Purpose

This file owns the line stored in the dated log file and the five project log content types in §2.2. `requirement-cpp-logger-folder.md` owns the directory. `requirement-cpp-output.md` owns the terminal line, which stream it uses, and what quiet and JSON hide. The running program still prints through `ciao_*` in `src/main/cpp/output.cpp`. This row is not that terminal line.

### 1.1 Human-facing

**In one sentence:** The stored line reads `[YYYY-MM-DD HH:MM:SS] pid:<pid> [LEVEL] @<file> :] message`, where `<file>` is the source file that logged it, and the file that holds the line is `gemini-wilgat-<YYYYMMDD>.log`.

| Box | Meaning | Example |
|-----|---------|---------|
| You / this login | The person who starts the program | You start it with no flags |
| The other role | `output.cpp` | It appends the stored line when a `ciao_*` call runs |
| Not this file | The terminal text, quiet, and JSON | `requirement-cpp-output.md` |

| Includes | Excludes |
|----------|----------|
| The stored line, the file `gemini-wilgat-<YYYYMMDD>.log`, and the rehearsal commands `test`, `list`, and `show` | The terminal tokens `[INFO]`, `[WARN]`, `[ERROR]`, and `[DEBUG]`, and which directory the login selects |

| Surface | What you open | What for |
|---------|---------------|----------|
| The log file | `gemini-wilgat-<YYYYMMDD>.log` | Read the stored line |
| `src/main/cpp/output.cpp` | `ciao_log` | The call that appends |
| `--log-datestamp` | Eight digits | Choose the date in the file name |

| You do… | What it means | What you type |
|---------|---------------|---------------|
| Rehearse one line | One practice line is stored under the directory you name | `gemini-wilgat test --home /tmp --log-datestamp 20261003` |

## 2. Core rules

- Product code keeps calling `ciao_info`, `ciao_warn`, `ciao_error`, and `ciao_debug`. Those four names are macros. Each macro passes the caller's source file into its `ciao_*_at` function, and that function calls `ciao_log`. Product code does not call `ciao_*_at` or `ciao_log`. There is no second logging API and no `logger_*` family.
- The translation unit is `src/main/cpp/output.cpp`. The header is `src/main/cpp/output.h`.
- `ciao_log` takes `va_list` by value. It uses `va_copy` before `g_strdup_vprintf`. The signature does not go back to `...`.
- A non-empty `DEBUG` enables `ciao_debug`. That predicate is `requirement-cpp-debug-mode.md`. There is no separate debug switch function.

### 2.1 Stored line

Local time, the process id, the level, the component, then the message. One line, one trailing newline. This shape is the file. The terminal shape stays `requirement-cpp-output.md`.

```text
[YYYY-MM-DD HH:MM:SS] pid:<pid> [<LEVEL>]<component> :] <message>
```

| Field | Rule |
|-------|------|
| `YYYY-MM-DD HH:MM:SS` | Local civil time from `localtime`. Each number is zero-padded. The separator between date and time is one space. Clock failure uses `1970-01-01 00:00:00`. |
| `<pid>` | Decimal `getpid()`, no padding and no sign. |
| `<LEVEL>` | Uppercase token inside one pair of brackets. The tokens are `INFO`, `DEBUG`, `WARNING`, and `ERROR`. `ciao_warn` stores `WARNING`, not `WARN`. There is no `FATAL` call and no omitted level. |
| `<component>` | One space, `@`, and the base name of the source file that contains the `ciao_*` call, with no space before `:]`. The base name keeps `.cpp`. A directory prefix is not stored. `src/main/cpp/output.cpp` stores `@output.cpp`. `src/main/cpp/userscript_manager.cpp` stores `@userscript_manager.cpp`. Two calls in the same file store the same component. A missing file name stores nothing after the level brackets. |
| ` :] ` | The four characters space, colon, close-bracket, space. They stay even when `<component>` is empty. |
| `<message>` | The formatted text only. No extra brackets and no second timestamp. |

```text
[2026-10-03 17:35:02] pid:1001 [INFO] @output.cpp :] Gemini (wilgat) Output System initialized (GNOME compliant)
[2026-10-03 17:35:02] pid:1001 [WARNING] @ui_window_app.cpp :] Bookmark index 1 out of range, falling back to Start Page
[2026-10-03 17:35:02] pid:1001 [ERROR] @main.cpp :] Critical: Failed to initialize WebKit
[2026-10-03 17:35:02] pid:1001 [DEBUG] @webkit_setup.cpp :] Starting WebKit setup...
```

Quiet and JSON do not change this stored line. They choose the terminal, which is `requirement-cpp-output.md`.

### 2.2 Project log content types

A project log content type is one kind of message this file stores. The five types are the catalog. A message that matches a type is stored. A message that matches none is not required in the dated file.

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

Quiet and JSON do not drop a matching line from the file. The stored shape stays §2.1. The directory stays `requirement-cpp-logger-folder.md`.

### 2.3 Dated file

The file is `gemini-wilgat-<YYYYMMDD>.log` inside the log directory, or inside the rehearsal log directory when the command is `test`, `list`, or `show`. Those directories are `requirement-cpp-logger-folder.md`. The check that reads the file is `TP-CPP-LOGGER` in `docs/reviews/test-plan.md` and `requirement-cpp-tests.md`.

| Environment or switch | File for `20261003` |
|-----------------------|---------------------|
| Root or sudo | `/var/gemini-wilgat/log/gemini-wilgat-20261003.log` |
| Other | `$HOME/.app/gemini-wilgat/log/gemini-wilgat-20261003.log` |
| `test --prefix /tmp` | `/tmp/var/gemini-wilgat/log/gemini-wilgat-20261003.log` |
| `test --home /tmp` | `/tmp/.app/gemini-wilgat/log/gemini-wilgat-20261003.log` |

1. The file name is `gemini-wilgat-<YYYYMMDD>.log`. It sits in the log directory on a normal run and in the rehearsal log directory on `test`, `list`, and `show`. `--home /tmp` and `--prefix /tmp` do not share a file.
2. `--log-datestamp` is eight digits. Any other shape is rejected with `--log-datestamp requires YYYYMMDD`. When the switch is absent on a normal run, the stamp is today's local date, `YYYYMMDD`. Clock failure uses `19700101`.
3. The `test` command and the `show` command require the switch. A missing switch is rejected with `test purpose command requires --log-datestamp YYYYMMDD`. They do not substitute today's date. `list` does not require it.
4. Each event that `ciao_log` emits is appended to this file as the stored line in §2.1. A **gnome-debug** line is one of those events. The output module records it. A direct `g_warning` that never reaches `ciao_log` does not satisfy §2.2. The console line is unchanged. The program creates the log directory when it appends. It does not use `mkstemp`. `list` and `show` do not append a line.
5. When the log directory is unresolved, no file is created.
6. `list` prints the basenames of regular files whose names end in `.log`, one per line, in sorted order. A directory or other non-regular name is omitted. A missing directory prints no names and exits 0. It does not create the directory. A directory that exists but cannot be opened exits 1. `list` and `show` do not change the file.
7. `show` prints the contents of `gemini-wilgat-<YYYYMMDD>.log` for the given datestamp. A missing file exits 1. `list` and `show` use the same directory switches as `test`. `--prefix` wins when both directory switches are present. They do not read `PREFIX`, `HOME`, or the effective user id. Quiet, JSON, and a missing verbose flag do not hide these prints. The display gate for terminal log lines is `requirement-cpp-output.md`.

`test`, `list`, and `show` are not in the help text while this row is Deferred. `requirement-cpp-parameters.md` keeps the flags the program accepts today. The live `log` verb is `requirement-cpp-log-commands.md`. It is not these three commands, and it does not accept `--log-datestamp`, `--home`, or `--prefix`.

### 2.4 Display

The stored line in §2.1 is not rewritten for display. The non-UI command `log show N` prints the corresponding file's bytes with no added header and no removed character. That command is `requirement-cpp-log-commands.md` §2.2. The rehearsal `show` in §2.3 item 7 does the same for the dated file it names: it prints the file, and it does not build a table.

The UI view for an app log is the HTML table in the main web view. It is not this Deferred row. The current rule is `requirement-cpp-log-commands.md` §2.8. The sheet omits `[` `]` `<` `>` `(` `)` `{` `}` from each cell. The file keeps those characters.

## 3. Protection rule

Future editors must not add a second logging API, change the §2.1 stored line, drop a type from §2.2, leave a `Gtk-CRITICAL` or `GLib-GObject-CRITICAL` line only on the terminal, stamp every line with one fixed component, put the date in another shape, write the file outside the log directory, or create a temporary file to hold the log. They must not make `log show` or the rehearsal `show` rewrite the file into the HTML table.

## 4. Related artifacts

| Artifact | Role |
|----------|------|
| `docs/requirements/index.md` | Registry |
| `docs/requirements/requirement-cpp-output.md` | Terminal streams, quiet, and JSON |
| `docs/requirements/requirement-cpp-logger-folder.md` | The log directory |
| `docs/requirements/requirement-cpp-tests.md` | `TP-CPP-LOGGER` |
| `docs/reviews/test-plan.md` | Status of `TP-CPP-LOGGER` |
| `docs/requirements/requirement-cpp-debug-mode.md` | When `ciao_debug` is enabled |
| `docs/requirements/requirement-cpp-parameters.md` | Flags the program accepts today |
| `src/main/cpp/output.cpp` | `ciao_log` |
| `src/main/cpp/output.h` | `ciao_*` |
| `docs/requirements/requirement-cpp-log-commands.md` | Live `log` verb. Not `test`, `list`, or `show` |
| `docs/requirements/requirement-cpp-log-format.md` | Active checker for §2.1. Not the writer |

**Last updated**: 2026-10-06
**Owner**: Logger
