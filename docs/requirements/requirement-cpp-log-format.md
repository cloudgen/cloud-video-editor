**file**: docs/requirements/requirement-cpp-log-format.md
**Status**: Active
**Updated**: 2026-10-06
**Philosophy**: CIAO v2.10.2 / CIAO-Lite (Caution • Intentional • Anti-fragile • Over-engineered / Over-protect)

## 1. Purpose

This file owns the checker that decides whether a log file follows the stored line, the test-purpose verb `logcheck`, and the address-bar Go path for a local file whose name ends in `.log`. The writer of the stored line stays `requirement-cpp-logger.md`. The HTML table stays `requirement-cpp-log-commands.md` §2.8. The rehearsal verbs `test`, `list`, and `show` stay unimplemented and stay out of the help text.

### 1.1 Human-facing

**In one sentence:** You run `logcheck`, or you type a local `.log` path and press Go, and a file that follows the stored line is the one the window can show as the log table.

| Box | Meaning | Example |
|-----|---------|---------|
| You / this login | The person checking a log file | You pass a rehearsal directory, or you type the file path |
| The other role | `logcheck` and the Go button | They use one checker and do not write a log line |
| Not this file | The `log` verb, and the rehearsal verbs `test`, `list`, and `show` | `requirement-cpp-log-commands.md` |

| Includes | Excludes |
|----------|----------|
| The stored-line checker, `logcheck`, and Go on a local `.log` file | `log list`, `log show N`, creating the log directory, and `test` / `list` / `show` |

| Surface | What you open | What for |
|---------|---------------|----------|
| The terminal | `gemini-wilgat logcheck ...` | Pass or fail, then stop |
| The address bar | A local `.log` path, then Go | The same table as File → Log when the checker passes |

| You do… | What it means | What you type |
|---------|---------------|---------------|
| Check one file | Pass prints the path and exits 0. Fail names the line and exits 1 | `gemini-wilgat logcheck --file /tmp/gemini-wilgat-20261003.log` |
| Aim a rehearsal | The dated file is chosen by the directory switch and the eight-digit stamp. The login home is not read | `gemini-wilgat logcheck --home /tmp --log-datestamp 20261003` |
| Open it in the window | Go loads the log table only after a pass | Type `/tmp/gemini-wilgat-20261003.log` and press Go |

## 2. Core rules

The checker and `logcheck` do not call `ciao_*`. They do not create a directory. They do not read `HOME`, `PREFIX`, or the effective user id. `--quiet` and `QUIET=1` do not hide the result. `--json` and `JSON=1` select one JSON object on stdout. The result does not go through `ciao_log`.

`ciao_output_init()` exits after flags are applied, before the output-system startup lines, and before `gtk_init()`, when `logcheck` is present. The process does not open a window.

A stored line is one line. This is the same shape `requirement-cpp-logger.md` §2.1 describes. A change to that shape changes this checker in the same edit.

```text
[YYYY-MM-DD HH:MM:SS] pid:<pid> [LEVEL] @<file> :] <message>
```

| Field | Pass rule |
|-------|-----------|
| `YYYY-MM-DD HH:MM:SS` | Nineteen characters, then `]`. Year is four digits. Month is `01`–`12`. Day is `01`–`31`. Hour is `00`–`23`. Minute is `00`–`59`. Second is `00`–`60`. One hyphen, one hyphen, one space, one colon, one colon. No space before `]`. |
| `pid:<pid>` | The five characters ` pid:` and then one or more digits. No sign and no space inside the digits. |
| `[LEVEL]` | The token is exactly `INFO`, `WARNING`, `ERROR`, or `DEBUG`. `WARN` does not pass. No space before `]`. |
| `@<file>` | Optional. When present: one space, `@`, then a base name. The base name is not empty, `.`, or `..`, and it contains no space and no `/`. There is no space before `:]`. When the base name is absent, the level bracket is followed directly by ` :] `. |
| ` :] ` | The four characters space, colon, close-bracket, space. |
| `<message>` | The rest of the line. It may be empty. Brackets and parentheses stay. They are not a reason to fail. |

A file passes when every non-empty line is a stored line and at least one stored line is present. A final empty piece after the trailing newline is not a line. A blank line before that final piece fails. A line that is not the stored shape fails. The file is not rewritten.

A passing file:

```text
[2026-10-03 17:35:02] pid:1001 [INFO] @output.cpp :] Gemini (wilgat) Output System initialized (GNOME compliant)
[2026-10-03 17:35:02] pid:1001 [WARNING] @ui_window_app.cpp :] Bookmark index 1 out of range, falling back to Start Page
[2026-10-03 17:35:02] pid:1001 [INFO] :] no component
```

A failing file:

```text
[2026-10-03 17:35:02] pid:1001 [WARN] @output.cpp :] short level token
not a stored line
```

The first failure is `line 1 does not follow the stored line`. An empty file fails with `no stored line`.

The read is the same regular-file read as `log show`: a symbolic link is not followed. A missing file, a directory, or an unreadable file is `cannot read log file <basename>`, not a format fail.

### 2.1 `logcheck`

`logcheck` is a test-purpose verb. Help lists it under `Tests:`, apart from `log` and `luascript`. It is not an action of `log`.

One of these forms:

| Form | File that is checked |
|------|----------------------|
| `logcheck --file <path>` | That path. The base name must end in `.log`. |
| `logcheck --home <dir> --log-datestamp <YYYYMMDD>` | `<dir>/.app/gemini-wilgat/log/gemini-wilgat-<YYYYMMDD>.log` |
| `logcheck --prefix <dir> --log-datestamp <YYYYMMDD>` | `<dir>/var/gemini-wilgat/log/gemini-wilgat-<YYYYMMDD>.log` |

`--prefix` wins when both directory switches are present. The other directory is not read. `--file` together with `--home` or `--prefix` exits 1 with `logcheck accepts either --file or --home or --prefix`. `--file` together with `--log-datestamp` exits 1 with `logcheck --file does not use --log-datestamp`.

`--log-datestamp` on the directory forms is eight digits. Any other shape exits 1 with `--log-datestamp requires YYYYMMDD`. A missing stamp exits 1 with `test purpose command requires --log-datestamp YYYYMMDD`. The command does not substitute today's date.

Flags may use a separate argument or one `=` argument (`--home /tmp` or `--home=/tmp`). `--json`, `--quiet`, `--config`, `--shutdown=`, and `--bookmark=` may sit before or after the verb. Any other flag exits 1 with `unexpected argument <token>`. An extra word exits 1 with `unexpected argument <token>`. No target exits 1 with `logcheck requires --file or --home or --prefix`. A base name that does not end in `.log` exits 1 with `not a log file <basename>`.

Text pass, exit 0:

```text
log format pass <path>
```

Text format fail, exit 1, on stdout:

```text
log format fail <path>: line 2 does not follow the stored line
```

Text usage and read errors go to stderr through `g_printerr` and exit 1. JSON is one object on stdout. A pass:

```json
{"command":"logcheck","result":"pass","path":"/tmp/.app/gemini-wilgat/log/gemini-wilgat-20261003.log"}
```

A format fail:

```json
{"command":"logcheck","result":"fail","path":"/tmp/sample.log","error":"line 2 does not follow the stored line"}
```

A usage or read error:

```json
{"command":"logcheck","error":"cannot read log file gemini-wilgat-20261003.log"}
```

`logcheck` alone is not a usage page. It exits 1 with `logcheck requires --file or --home or --prefix`.

### 2.2 Address bar

The Go button is `on_go_button_clicked()` in `src/main/cpp/url_bar_go_button.cpp`. After the entry and the web view are valid, a local `.log` request is checked before `load_page()`.

A local `.log` request is either:

| Request | Meaning |
|---------|---------|
| `file:` URI | The decoded path ends in `.log` |
| Absolute path | The text starts with `/` and ends in `.log` |

`http` and `https` are not this request. They still call `load_page()`.

When the request is a local `.log` file:

| Result | What the window does |
|--------|----------------------|
| The file cannot be read | A dialog says `cannot read log file <basename>`. `load_page()` is not called. |
| The checker fails | A dialog says `log file does not follow the stored line`. The log table is not loaded. `load_page()` is not called. |
| The checker passes | The main web view loads the same HTML document as File → Log → a numbered file. The function is `log_view_load_in_web_view()`. The base URI is the file URI of that path. The address bar is set to that URI. |

The document is `requirement-cpp-log-commands.md` §2.8: header `Time`, `Pid`, `Level`, `Source`, `Message`; odd rows `#ffffff`; even rows `#e7eef6`; a row under the pointer `#ffe08a`. Displayed cells omit `[` `]` `<` `>` `(` `)` `{` `}`. Source drops a trailing `.cpp` or `.c`. The file is not rewritten.

File → Log does not run this checker. A numbered file in the log folder still loads §2.8, including a line that is not the stored shape. A file that passes this checker has the same table from the menu and from Go.

This path does not call `ciao_*`, so opening a log does not append a line.

### 2.3 Why this requirement exists

- **CIAO Principle 1 – Caution:** a `.log` name is not treated as this product's log until the bytes match the stored line.
- **CIAO Principle 5 – Single source:** the menu and Go call one HTML function.
- **CIAO Principle 16 – Input pattern:** the checker is the pattern. The verb and Go both use it.

## 3. Design principles

- **Caution:** a blank line, a short level token, and an empty file fail.
- **Intentional:** `logcheck` is listed under Tests, apart from `log` and `luascript`.
- **Anti-fragile:** a failed check leaves the current page in place and does not call `load_page()`.
- **Over-protect:** the rehearsal path is built from the switches only. `HOME`, `PREFIX`, and the effective user id are not read.

## 4. Protection rule

Future editors must not route this result through `ciao_log`, create a directory from `logcheck` or from Go on a `.log` file, implement `test`, `list`, or `show` here, or list `logcheck` in the operational command block. They must not show a failing file with the log table, and they must not give Go a second table. They must not make `logcheck` read `HOME`, `PREFIX`, or the effective user id.

## 5. Verification

| TP-ID | Pass |
|-------|------|
| TP-LOG-FORMAT | A file of stored lines, including one line with no `@<file>` and one message that contains parentheses, makes `logcheck --file` print `log format pass` and exit 0. A `WARN` token and a plain second line make it print `log format fail` and exit 1. An empty file fails with `no stored line`. `--home` and `--prefix` with the same stamp name two different paths and do not create either directory. A missing file exits 1 with `cannot read log file` and does not create the directory. A missing stamp exits 1 with `test purpose command requires --log-datestamp YYYYMMDD`. Eight digits are required. `--json` prints one object. Help lists `logcheck` under `Tests:` and does not list `test`, `list`, or `show`. The process does not open a window. Go on a passing local `.log` path uses `log_view_load_in_web_view()`. |

Map: `docs/reviews/test-plan.md`.

## 6. Related artifacts

| Artifact | Role |
|----------|------|
| `docs/requirements/index.md` | Registry |
| `docs/requirements/requirement-cpp-logger.md` | Stored line. Deferred writer |
| `docs/requirements/requirement-cpp-log-commands.md` | The HTML table, §2.8. The `log` verb is not `logcheck` |
| `docs/requirements/requirement-cpp-parameters.md` | Help row for `logcheck` |
| `docs/requirements/requirement-cpp-tests.md` | `TP-LOG-FORMAT` |
| `docs/reviews/test-plan.md` | Status of `TP-LOG-FORMAT` |
| `src/main/cpp/log_format.cpp` | The checker and the verb |
| `src/main/cpp/url_bar_go_button.cpp` | Go on a local `.log` file |
| `src/main/cpp/menu_log.cpp` | `log_view_load_in_web_view()` |
| `src/main/cpp/log_view_html.cpp` | The HTML table |
| `src/main/cpp/output.cpp` | Early exit and the Tests heading |

**Last updated**: 2026-10-06
**Owner**: Log format
**Alignment**: Registry `docs/requirements/index.md`; **CIAO** (https://github.com/cloudgen/ciao); CIAO-Lite (https://github.com/cloudgen/ciao-lite).
