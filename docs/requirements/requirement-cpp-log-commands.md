**file**: docs/requirements/requirement-cpp-log-commands.md
**Status**: Active
**Updated**: 2026-10-06
**Philosophy**: CIAO v2.10.2 / CIAO-Lite (Caution • Intentional • Anti-fragile • Over-engineered / Over-protect)

## 1. Purpose

This file owns the File → Log menu and the `log` command. The stored line stays in `requirement-cpp-logger.md`. The directory stays in `requirement-cpp-logger-folder.md`. Both of those rows stay Deferred. The rehearsal commands `test`, `list`, and `show` stay unimplemented and stay out of the help text.

### 1.1 Human-facing

**In one sentence:** You open File → Log, or you run `log`, to list numbered log files, open one by number, open the log folder, or clear the log files.

| Box | Meaning | Example |
|-----|---------|---------|
| You / this login | The person reading logs | You choose file 1 or run `log show 1` |
| The other role | The Log menu and the `log` verb | They read the same folder and do not create it |
| Not this file | The stored line, and the rehearsal verbs `test`, `list`, and `show` | `requirement-cpp-logger.md` |

| Includes | Excludes |
|----------|----------|
| The Log menu, `log list`, `log show N`, `log folder`, `log clear`, and `--json` for that verb | `--log-datestamp`, `--home`, `--prefix`, and a second logger |

| Surface | What you open | What for |
|---------|---------------|----------|
| File → Log | The menu | List, folder, and clear |
| The terminal | `gemini-wilgat log ...` | The same actions, with or without `--json` |

| You do… | What it means | What you type |
|---------|---------------|---------------|
| List files | A 1-based list of `*.log` names | `log list`, or File → Log → List log files |
| Read one | The terminal prints the file bytes. The menu shows the HTML table in the main web view | `log show 1`, or the numbered item |
| See the folder | The terminal prints the path, even when the folder is missing. The menu opens that path in the desktop file manager | `log folder`, or Log folder |
| Clear | Delete regular `*.log` files only | `log clear`, or Clear log |

## 2. Core rules

The directory is the one already resolved by `logger_resolve_dir()` in `src/main/cpp/output.cpp`. Root or sudo (effective user id 0) uses `/var/gemini-wilgat/log`. Every other user uses `$HOME/.app/gemini-wilgat/log`. An empty or unset `HOME` on a non-root process leaves the directory unresolved. These commands do not accept `--home`, `--prefix`, or `--log-datestamp`. They do not call `getpwuid`. They do not create the directory.

A log file is a regular file in that directory whose basename ends in `.log`. A name containing `/` is ignored. `.` and `..` are not entries. A symbolic link is not a regular file and is left in place. Any other name is left in place. The directory itself is not deleted.

Names are sorted by basename ascending, byte order. Numbers start at 1 in that order.

The verb is the argument `log`. Flags may sit before it, after it, or between it and the action (`--json log list`, `log list --json`, `log --json show 2`). `--json` and `JSON=1` select one JSON object on stdout for this verb. `--quiet` and `QUIET=1` do not hide the result. The result does not go through `ciao_log`, so it is not stored and it does not create the directory.

`ciao_output_init()` exits after flags are applied, before the output-system startup lines, and before `gtk_init()`. `gemini-wilgat log ...` does not open a window.

`log N` and `log list N` are not commands. The numbered read is `log show N` only.

### 2.1 `log list`

Text is one line per file, `1. <basename>`, then a newline. An empty list prints no lines and exits 0. JSON is one object:

```json
{"command":"log","action":"list","folder":"/path","files":[{"number":1,"name":"gemini-wilgat-20261004.log"}]}
```

A missing directory is an empty list, exit 0, and the JSON object still includes `folder`. An unresolved directory exits 1. A path that exists but cannot be read exits 1.

### 2.2 `log show N`

`N` is the number from the same ordering as `log list`. Text is the raw file bytes, with no added header and no removed character. Enclosure characters such as `[` `]` `<` `>` `(` `)` `{` `}` stay in that text. JSON is one object:

```json
{"command":"log","action":"show","number":1,"name":"gemini-wilgat-20261004.log","content":"..."}
```

`content` escapes `\`, `"`, and control characters (`\n`, `\r`, `\t`, and `\u00XX` for other controls). A missing number, `0`, a non-numeric token, or a number past the list exits 1 with `no log file <token>`.

### 2.3 `log folder`

Text is the directory path plus a newline. JSON is one object:

```json
{"command":"log","action":"folder","folder":"/path"}
```

The path is printed when the directory does not exist. An unresolved directory exits 1. This section is the terminal command. File → Log → Log folder is section 2.7. It opens a file manager and does not print this text.

### 2.4 `log clear`

The command deletes each regular `*.log` file and does not ask. Text is `Cleared N log file(s) from <folder>`. JSON is one object:

```json
{"command":"log","action":"clear","folder":"/path","removed":2}
```

A missing directory removes 0 and exits 0. An unresolved or unreadable directory exits 1. A delete that fails exits 1. The error names the first file that could not be deleted: `cannot delete log file <basename>`.

### 2.5 `log` with no action

Text is a short usage of `list`, `show N`, `folder`, and `clear`, and the process exits 0. JSON is one object:

```json
{"command":"log","actions":["list","show","folder","clear"]}
```

Any other action exits 1 with `unknown log action <token>`. An extra token exits 1 with `unexpected argument <token>`. `log show` without a number exits 1 with `missing log file number`.

### 2.6 Errors

A JSON error is one object on stdout, and the process still exits 1:

```json
{"command":"log","action":"show","error":"no log file 3"}
```

Text errors go to stderr through `g_printerr`. They do not use `ciao_error`. The sentences are:

| Condition | Text |
|-----------|------|
| Unresolved directory | `log folder is unresolved` |
| Unreadable directory | `cannot read log folder` |
| Bad or missing file number | `no log file <token>` |
| `log show` without a number | `missing log file number` |
| File bytes cannot be read | `cannot read log file <basename>` |
| Delete failed | `cannot delete log file <basename>` |
| Unknown action | `unknown log action <token>` |
| Extra token | `unexpected argument <token>` |

### 2.7 File menu

`build_main_menu_bar()` appends Log before Exit. The item is present when bookmarks are disabled. The menu does not use `--json`. The submenu is, in order:

| Label | Behavior |
|-------|----------|
| List log files | A submenu rebuilt when the item is selected. Entries are `1. <basename>`, `2. <basename>`, and so on. Choosing one loads §2.8 into the main web view. An empty list shows one insensitive item, `No log files`. An unresolved directory shows one insensitive item, `Log folder is unresolved`. An unreadable directory shows one insensitive item, `Cannot read log folder`. A file that cannot be read says `cannot read log file <basename>`. A missing web view says `Cannot show log`. |
| Log folder | Opens the path in the desktop file manager. The default application for `inode/directory` is used, then `xdg-open` when that application is missing. The directory is not created. An unresolved directory says `Log folder is unresolved`. A launch that fails says `Cannot open log folder`. |
| Clear log | When the count is 0, a message `No log files to clear`. Otherwise confirm `Delete N log file(s) from <folder>?`. No does nothing. Yes deletes and then shows `Cleared N log file(s) from <folder>`. |

### 2.8 UI view for an app log

Choosing a numbered file loads one HTML document into the main web view. The document is the [ui-view-for-app-log](../terminologies/ui-view-for-app-log.md). This section copies that rule for this product. `log show N` is not this view. Section 2.2 still prints the file bytes.

The document is one `table`. The header row is `Time`, `Pid`, `Level`, `Source`, `Message`. The header background is `#1f4e79` with white bold text. Odd body rows use `#ffffff`. Even body rows use `#e7eef6`. A body row under the pointer uses `#ffe08a`.

A stored line fills the five cells:

```text
[YYYY-MM-DD HH:MM:SS] pid:<pid> [LEVEL] @<file> :] <message>
```

`Time` is the civil timestamp. `Pid` is the digits. `Level` is the token. `Source` is `<file>` without the `@`. A trailing `.cpp` or `.c` is left off that cell. `@output.cpp` shows `output`. `@widget.c` shows `widget`. Any other ending stays, still without the `@`. A missing `@<file>` leaves `Source` empty. `Message` is the text after ` :] `. A line that is not that shape is one body row whose `Message` cell is the line and whose other cells are empty. A blank line is not a row. The stored line still keeps `.cpp` or `.c`.

Every displayed cell omits `[` `]` `<` `>` `(` `)` `{` `}`. The file is not rewritten. The base URI of the document is the file URI of that log file, so the address bar shows that URI.

The address bar Go button is not this menu. A local path or `file:` URI whose name ends in `.log` is `requirement-cpp-log-format.md`. When that checker passes, Go loads this same document through `log_view_load_in_web_view()`. When it does not pass, Go does not load this document. This menu does not run that checker.

## 3. Protection rule

Future editors must not route this result through `ciao_log`, create the directory from these commands, implement `test`, `list`, or `show` in this file, accept `log N` as a shortcut, add a Log item anywhere except `build_main_menu_bar()`, or name one desktop file manager. The menu uses the `inode/directory` application, then `xdg-open`. They must not show a chosen log file in a text-view dialog, strip enclosure characters from `log show`, or give the table stripes and the hover the same background.

## 4. Verification

| TP-ID | Pass |
|-------|------|
| TP-LOG-COMMANDS | With `HOME` set to an empty temporary directory, `log folder` prints `$HOME/.app/gemini-wilgat/log` and does not create it. `log list` prints nothing and exits 0. After two regular `*.log` files and one non-log file, `log list` numbers the log files in basename order, `log show 1` prints that file unchanged, including `[` `]` `<` `>` `(` `)` `{` `}`, `log show 9` exits 1, and `log clear` deletes only the `*.log` files. `--json` prints one object for list, show, folder, and clear. `log` alone exits 0. The process does not open a window. |
| TP-UI-VIEW-APP-LOG | The HTML for one stored line and one other line is one table. The header cells are Time, Pid, Level, Source, and Message. Odd rows are `#ffffff`, even rows are `#e7eef6`, and hover is `#ffe08a`. No cell text contains `[` `]` `<` `>` `(` `)` `{` `}`. The stored line `Milk low (oat)` shows Message `Milk low oat` and Source `output` for component `@output.cpp`. Component `@widget.c` shows Source `widget`. The other line is Message only. `log show` of that file still prints the brackets and the parentheses. The menu loads that HTML into the main web view. |

Map: `docs/reviews/test-plan.md`. The File → Log click path stays with `TP-UI-MENU`, which stays todo until that menu check is run.

## 5. Related artifacts

| Artifact | Role |
|----------|------|
| `docs/requirements/index.md` | Registry |
| `docs/requirements/requirement-cpp-logger.md` | Stored line, Deferred. Not this verb |
| `docs/requirements/requirement-cpp-logger-folder.md` | Directory, Deferred |
| `docs/requirements/requirement-cpp-output.md` | `ciao_*` lines. This result is not one of them |
| `docs/requirements/requirement-cpp-parameters.md` | Help row for `log` |
| `docs/requirements/requirement-cpp-ui-menu.md` | File order, Log before Exit |
| `docs/requirements/requirement-cpp-tests.md` | `TP-LOG-COMMANDS` |
| `docs/reviews/test-plan.md` | Status of `TP-LOG-COMMANDS` |
| `src/main/cpp/output.cpp` | Directory getter and early exit |
| `src/main/cpp/log_commands.cpp` | The verb |
| `src/main/cpp/menu_log.cpp` | The Log menu |
| `src/main/cpp/log_view_html.cpp` | The HTML table |
| `docs/requirements/requirement-cpp-log-format.md` | Checker and Go. A pass uses this table |
| `src/main/cpp/ui_menu_app.cpp` | Inserts Log before Exit |

**Last updated**: 2026-10-06
**Owner**: Log commands
