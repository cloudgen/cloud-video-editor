**file**: docs/requirements/requirement-cpp-lua-script-commands.md
**Status**: Active
**Updated**: 2026-10-04
**Philosophy**: CIAO v2.10.2 / CIAO-Lite (Caution • Intentional • Anti-fragile • Over-engineered / Over-protect)

## 1. Purpose

This file owns the File → Lua Script menu and the `luascript` command. The directory and which scripts run stay in `requirement-cpp-window.md`. The sol2 host stays in `requirement-cpp-lua-script.md`. The `[scripts]` keys stay in `requirement-cpp-configuration.md`. This verb is operational. It is not a test command.

The menu item and the verb are required behavior. They are in the binary.

### 1.1 Human-facing

**In one sentence:** You open File → Lua Script, or you run `luascript`, to see the lua script folder and to view, edit, or create a `.lua` file stored there.

| Box | Meaning | Example |
|-----|---------|---------|
| You / this login | The person editing scripts | You choose New lua script and type `gemini-note.lua` |
| The other role | The Lua Script menu and the `luascript` verb | They read the same folder the loader uses |
| Not this file | Which scripts run, and the sol2 host | `requirement-cpp-window.md` and `requirement-cpp-lua-script.md` |

| Includes | Excludes |
|----------|----------|
| The Lua Script menu, `luascript folder`, `luascript list`, `luascript view`, `luascript edit`, `luascript new`, and the dialog that asks for the new filename | `@match`, `@priority`, the sol2 libraries, and a second script directory |

| Surface | What you open | What for |
|---------|---------------|----------|
| File → Lua Script | The menu | The folder, the stored scripts, and New |
| The New lua script dialog | A filename entry | The name of the file to create |
| The terminal | `gemini-wilgat luascript ...` | The same actions, with or without `--json` |

| You do… | What it means | What you type |
|---------|---------------|---------------|
| See the folder | The path is shown even when the folder is missing | `gemini-wilgat luascript folder`, or Lua script folder |
| See the scripts | A 1-based list of the `.lua` files in that folder | `gemini-wilgat luascript list`, or List lua scripts |
| Read one | The bytes of that file | `gemini-wilgat luascript view 1` |
| Change one | The menu editor saves the text view, or stdin replaces the file | Edit, then Save, or `gemini-wilgat luascript edit gemini-log.lua` |
| Create one | The dialog asks for the filename, then opens the new file | File → Lua Script → New lua script, or `gemini-wilgat luascript new gemini-note.lua` |

## 2. Core rules

The directory is the loader directory from `requirement-cpp-window.md`. For this product the default is `~/.app/gemini-wilgat/userscripts`. When `config.conf` exists, `[scripts] path` replaces that default. The menu reads the path through `config_get_scripts_path()` and `resolve_full_storage_path()` after configuration has loaded. `[scripts] enabled` does not hide the menu and does not reject the verb.

A lua script file is a regular file in that directory whose final extension is `.lua`, the same test the loader uses. A name containing `/` is ignored. `.` and `..` are not entries. A symbolic link is not a regular file. Any other name is left in place. The directory itself is not deleted.

Names are sorted by basename ascending, byte order. Numbers start at 1 in that order. That order is the menu and the verb. The loader's priority order stays in `requirement-cpp-window.md`.

The verb is the argument `luascript`. Flags may sit before it, after it, or between it and the action (`--json luascript list`, `luascript list --json`, `luascript --json view 2`). `--config` consumes the next argument. Other arguments that start with `--` are flags, not actions. `--json` and `JSON=1` select one JSON object on stdout for this verb. `--quiet` and `QUIET=1` do not hide the result. The result does not go through `ciao_log` or `ciao_error`, so it is not stored and it does not create the log directory.

`ciao_output_init()` exits after flags are applied, before the output-system startup lines, and before `gtk_init()`. `gemini-wilgat luascript ...` does not open a window.

`luascript N` and `luascript list N` are not commands. The numbered read is `luascript view`. The numbered replace is `luascript edit`.

`folder`, `list`, `view`, and `edit` do not create the directory and do not create `config.conf`. `new` creates the directory, mode `0700`, only after the filename is accepted and only when the directory is missing. `new` still does not create `config.conf`. The verb must not call `config_init()`.

A token that is only digits, with no leading zero, is a file number. `0` is a number and matches no file. Any other token is a basename. The file `1.lua` is selected with the token `1.lua`.

### 2.1 `luascript folder`

Text is the directory path plus a newline. JSON is one object:

```json
{"command":"luascript","action":"folder","folder":"/path"}
```

The path is printed when the directory does not exist. An unresolved directory exits 1.

### 2.2 `luascript list`

Text is one line per file, `1. <basename>`, then a newline. An empty list prints no lines and exits 0. JSON is one object:

```json
{"command":"luascript","action":"list","folder":"/path","files":[{"number":1,"name":"gemini-log.lua"}]}
```

A missing directory is an empty list, exit 0, and the JSON object still includes `folder`. An unresolved directory exits 1. A path that exists and cannot be read exits 1.

### 2.3 `luascript view`

`luascript view <token>` reads one file. `<token>` is a number from `luascript list` or a listed basename. Text is the raw file bytes. JSON is one object:

```json
{"command":"luascript","action":"view","number":1,"name":"gemini-log.lua","content":"..."}
```

`content` escapes `\`, `"`, and control characters (`\n`, `\r`, `\t`, and `\u00XX` for other controls). A missing token exits 1 with `missing lua script name`. An unknown token exits 1 with `no lua script <token>`.

### 2.4 `luascript edit`

`luascript edit <token>` replaces one listed file. The new bytes are the whole of standard input. The file mode is left as it was. Text is `Updated <basename>`. JSON is one object:

```json
{"command":"luascript","action":"edit","number":1,"name":"gemini-log.lua","bytes":24}
```

`bytes` is the number of bytes written.

The menu text view is the editor. The verb does not ask for the script body one line at a time. When standard input is a terminal, the verb exits 1 with `lua script edit needs the new text on stdin` and does not read. A pipe, including an empty pipe, is the non-interactive path and must not wait. A missing token exits 1 with `missing lua script name`. An unknown token exits 1 with `no lua script <token>`. A failed write exits 1 with `cannot write lua script <basename>`.

### 2.5 `luascript new`

`luascript new <filename>` creates one file. The filename is one field. It is not a secret.

The dispatch decides once, before any read, whether a prompt is allowed. A prompt is allowed only when standard input and standard output are both terminals and JSON is off. The function that reads the line receives that decision and does not test the terminal itself. The prompt is one line on stderr, `Lua script filename: `, then one line from stdin. The trailing newline is removed. There is no second question.

When a prompt is not allowed, a missing filename exits 1 with `missing lua script filename` and does not read. `gemini-wilgat luascript new gemini-note.lua` does not prompt.

A token with no `.` gains a `.lua` suffix before the check. `gemini-note` is stored as `gemini-note.lua`. A token that contains `.` and does not end in `.lua` is rejected. The stored name must match `^[A-Za-z0-9][A-Za-z0-9._-]*\.lua$`. A slash, an empty name, `.`, `..`, or a name outside that pattern exits 1 with `lua script filename must be a single name`. A name that ends in another extension exits 1 with `lua script filename must end in .lua`.

An existing listed name, or any existing directory entry with that basename, exits 1 with `lua script already exists <basename>` and the bytes stay as they were.

The created file mode is `0600`. Its body is these bytes, including the final newline:

```lua
-- @match        https://gemini.google.com/*
-- @priority     50

function on_url_changed(url)
end

register_on_url_changed(on_url_changed)
```

What `@match` and `@priority` mean stays in `requirement-cpp-window.md`. This body is only the text `new` writes.

Text is `Created <basename>`. JSON is one object:

```json
{"command":"luascript","action":"new","folder":"/path","name":"gemini-note.lua"}
```

A directory that cannot be created exits 1 with `cannot create lua script folder`. A file that cannot be written exits 1 with `cannot write lua script <basename>`.

### 2.6 `luascript` with no action

Text is a short usage of `folder`, `list`, `view <token>`, `edit <token>`, and `new <filename>`, and the process exits 0. JSON is one object:

```json
{"command":"luascript","actions":["folder","list","view","edit","new"]}
```

Any other action exits 1 with `unknown lua script action <token>`. An extra token exits 1 with `unexpected argument <token>`. `--config` with no path exits 1 with `missing config path`.

### 2.7 Errors

A JSON error is one object on stdout, and the process still exits 1:

```json
{"command":"luascript","action":"new","error":"lua script already exists gemini-note.lua"}
```

Text errors go to stderr through `g_printerr`. They do not use `ciao_error`. The sentences are:

| Condition | Text |
|-----------|------|
| Unresolved directory | `lua script folder is unresolved` |
| Unreadable directory | `cannot read lua script folder` |
| Bad or missing file token | `no lua script <token>` |
| `view` or `edit` without a token | `missing lua script name` |
| File bytes cannot be read | `cannot read lua script <basename>` |
| Write failed | `cannot write lua script <basename>` |
| Directory create failed | `cannot create lua script folder` |
| Missing filename | `missing lua script filename` |
| Illegal filename | `lua script filename must be a single name` |
| Wrong extension | `lua script filename must end in .lua` |
| Name already present | `lua script already exists <basename>` |
| Edit from a terminal | `lua script edit needs the new text on stdin` |
| Missing `--config` path | `missing config path` |
| `--config` file missing | `cannot read config file <path>` |
| Unknown action | `unknown lua script action <token>` |
| Extra token | `unexpected argument <token>` |

### 2.8 File menu

`build_main_menu_bar()` appends Lua Script after User Agent and before Log. The item is present when bookmarks are disabled and when `[scripts] enabled` is false. The label is `Lua Script`. The menu does not use `--json`. These handlers do not call `ciao_*`. The submenu is, in order:

| Label | Behavior |
|-------|----------|
| List lua scripts | A submenu rebuilt when the item is selected. Entries are `1. <basename>`, `2. <basename>`, and so on. Choosing one opens the script dialog in section 2.10, read-only. An empty list shows one insensitive item, `No lua scripts`. An unresolved directory shows one insensitive item, `Lua script folder is unresolved`. An unreadable directory shows one insensitive item, `Cannot read lua script folder`. |
| Lua script folder | A message dialog. The title is `Lua script folder`. The body is the path. An unresolved directory says `Lua script folder is unresolved`. |
| New lua script | Opens the filename dialog in section 2.9. |

### 2.9 New lua script dialog

The dialog is modal, attached to the main window, and destroyed with that window. The title is `New lua script`. The content is a vertical box:

| Row | Widget |
|-----|--------|
| 1 | The label `Lua script filename` |
| 2 | The line `This file is created in the lua script folder. A name with no dot is saved with .lua added.` |
| 3 | A text entry. The placeholder is `name.lua`. The entry is empty. Activating the entry activates Create. |
| 4 | An error label. It is empty until Create fails. |

The buttons are Cancel and Create. Create is the default. Cancel, and closing the dialog, writes nothing. Create applies the filename rules in section 2.5. A failure leaves the dialog open, leaves the entry text in place, and puts the error sentence in the error label. A success closes this dialog, writes the file, creates the directory when it is missing, and opens the script dialog on that file with the text view editable.

### 2.10 Script dialog

The dialog is modal, attached to the main window, and destroyed with that window. The title is the basename. The default size is 720 by 480. The body is a scrolled monospace text view with wrapping off. The buttons are Edit, Save, and Close.

Opened from List lua scripts, the text view is not editable, Save is insensitive, and Edit is sensitive. The view shows the file as UTF-8, using the same valid-text conversion as the log file dialog. Edit makes the view editable and makes Save sensitive. Save writes the buffer as UTF-8 and leaves the view editable. A failed save leaves the dialog open and shows the error sentence.

When the buffer differs from the last saved text, Close asks `Save changes to <basename>?`. Yes writes and then closes. A failed write leaves the dialog open. No closes and leaves the file unchanged. Cancel returns to the dialog. When the buffer matches the last saved text, Close closes without asking.

Opened after New lua script, the view is editable, Save is sensitive, and the buffer matches the file just written, so Close does not ask until the text changes.

### 2.11 Implementation notes (this project)

The verb resolves the directory without `config_init()`:

1. `--config <path>` selects that file. A missing file exits 1. A missing path exits 1.
2. With no `--config`, `$HOME/.app/gemini-wilgat/config.conf` is read when it exists.
3. When no config file is read, the path is `~/.app/gemini-wilgat/userscripts`.
4. The `path` key in `[scripts]` is the directory when a config file is read.
5. A leading `~/` expands with `HOME`. Any other relative path is joined onto `HOME`, matching `resolve_full_storage_path()`. An absolute path stays absolute.
6. An unset or empty `HOME` leaves a `~/` path or a relative path unresolved.

Homes for the implementation:

| Path | Role |
|------|------|
| `src/main/cpp/lua_script_commands.cpp` | The verb |
| `src/main/cpp/menu_lua_script.cpp` | The Lua Script menu and both dialogs |
| `src/main/cpp/ui_menu_app.cpp` | Inserts Lua Script after User Agent and before Log |
| `src/main/cpp/output.cpp` | Early exit and the help lines |

`ciao_print_help()` gains these command lines:

```text
luascript folder     Display the lua script folder
luascript list       Number the lua scripts
luascript view N     Display lua script number N
luascript edit N     Replace lua script number N from stdin
luascript new NAME   Create a lua script
```

And these examples:

```text
gemini-wilgat luascript folder
gemini-wilgat luascript new gemini-note.lua
```

### 2.12 Why this requirement exists

- **CIAO Principle 5** (https://github.com/cloudgen/ciao): one directory. The menu and the verb read the loader path.
- **CIAO Principle 1**: a bad filename or a failed write is reported for that action, and the other files stay in place.
- **CIAO Principle 16** (https://github.com/cloudgen/ciao): the new-file name is one dialog field, and the terminal asks for it only on a terminal.

## 3. Design principles

- **Caution**: `folder` and `list` do not create the directory or `config.conf`. `new` does not replace an existing name.
- **Intentional**: the filename dialog is the create path. The script dialog is the view and edit path. The verb is the same actions without a window.
- **Anti-fragile**: one unreadable file is an error for that file. The list still builds from the files that can be named.
- **Over-protect**: the verb result is not a `ciao_*` line. The menu handlers do not call `ciao_*`.

## 4. Protection rule

Future editors must not route this result through `ciao_log` or `ciao_error`, call `config_init()` from this verb, create `config.conf` from this verb, create the script directory from `folder`, `list`, `view`, or `edit`, open a window from the verb, accept `luascript N` as a shortcut, hang when standard input is not a terminal, or add a Lua Script item anywhere except `build_main_menu_bar()`.

## 5. Verification

| TP-ID | Pass |
|-------|------|
| TP-LUA-SCRIPT-COMMANDS | With `HOME` set to an empty temporary directory, `luascript folder` prints `$HOME/.app/gemini-wilgat/userscripts` and does not create that directory or `config.conf`. `luascript list` prints nothing and exits 0. After two regular `*.lua` files and one non-lua file, `luascript list` numbers the lua files in basename order, `luascript view 1` prints that file, and `luascript view 9` exits 1. `luascript new gemini-note.lua` creates that file with the body in section 2.5, mode `0600`, and creates the directory mode `0700`. A second `luascript new gemini-note.lua` exits 1 and leaves the file unchanged. `luascript new` with standard input not a terminal exits 1 and does not read. `luascript edit` of a listed file with standard input not a terminal replaces the bytes. `luascript edit` with a terminal exits 1 and does not read a body. `--json` prints one object for folder, list, view, edit, and new. `luascript` alone exits 0. The process does not open a window. |

Map: `docs/reviews/test-plan.md`. The File → Lua Script click path stays with `TP-UI-MENU`, which stays todo until that menu check is run.

## 6. Related artifacts

| Artifact | Role |
|----------|------|
| `docs/requirements/index.md` | Registry |
| `docs/requirements/requirement-cpp-window.md` | Directory, and which scripts run |
| `docs/requirements/requirement-cpp-lua-script.md` | sol2 host |
| `docs/requirements/requirement-cpp-configuration.md` | `[scripts] path` and `[scripts] enabled` |
| `docs/requirements/requirement-cpp-project-structure.md` | `userscripts/` in the runtime tree |
| `docs/requirements/requirement-cpp-ui-menu.md` | File order, Lua Script before Log |
| `docs/requirements/requirement-cpp-parameters.md` | Help row for `luascript` |
| `docs/requirements/requirement-cpp-output.md` | This result is not a `ciao_*` line |
| `docs/requirements/requirement-cpp-tests.md` | `TP-LUA-SCRIPT-COMMANDS` |
| `docs/reviews/test-plan.md` | Status of `TP-LUA-SCRIPT-COMMANDS` |
| `src/main/cpp/userscript_manager.cpp` | Loader that already scans this directory |
| `src/main/cpp/lua_script_commands.cpp` | The verb |
| `src/main/cpp/menu_lua_script.cpp` | The Lua Script menu and both dialogs |
| `src/main/cpp/ui_menu_app.cpp` | Inserts Lua Script after User Agent and before Log |

**Last updated**: 2026-10-04
**Owner**: Lua script commands
