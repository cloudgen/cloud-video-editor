**file**: docs/requirements/requirement-cpp-configuration.md
**Status**: Active
**Updated**: 2026-10-02
**Philosophy**: CIAO v2.10.2 / CIAO-Lite (Caution • Intentional • Anti-fragile • Over-engineered / Over-protect)

## 1. Purpose

This file owns `config.conf`: where it lives, what happens when it is missing, and the rule that surrounding quotes on a value are stripped. The window that uses those flags is `requirement-cpp-window.md`.

### 1.1 Human-facing

**In one sentence:** On first launch the program writes `~/.app/gemini-wilgat/config.conf` for you, and you edit that file to change window size and storage names.

| Box | Meaning | Example |
|-----|---------|---------|
| You / this login | The person who edits the INI file | You set `default_width` |
| The other role | `config.cpp` | It creates the file and strips accidental quotes |
| Not this file | Bookmark URLs and the startup index | `requirement-cpp-webui-bookmark.md` |

| Includes | Excludes |
|----------|----------|
| Path, missing-file creation, quote stripping, default macros in `config.h` | WebKit setup order |

| Surface | What you open | What for |
|---------|---------------|----------|
| `~/.app/gemini-wilgat/config.conf` | User config | Runtime flags |
| `--config <path>` | Optional override | A different file |

| You do… | What it means | What you type |
|---------|---------------|---------------|
| Start with no config file | The directory and `config.conf` are created, then the window opens | launch the binary |
| Check that creation | TP-CONFIG-CREATE | see `docs/reviews/test-plan.md` |

## 2. Core rules

Source order, highest first:

1. Command-line arguments (`--config`, and the flags in `requirement-cpp-parameters.md`).
2. Environment variables from that same parameters file.
3. `config.conf`.
4. Skill config files, only if a future skill loader exists. None exist in the current binary.
5. Macros in `config.h`.

The config path is `~/.app/` plus `PROJECT_ID` plus `/config.conf`. The filename is `config.conf`, not `<project>.conf`. Override with `--config <path>`.

When the file is missing, unreadable, or malformed:

1. Use the `config.h` defaults.
2. Create `~/.app/<PROJECT_ID>/` if needed.
3. Write a new `config.conf` from those same macros.
4. Log the creation with `ciao_info()`.

The INI reader must strip one matching pair of `"` or `'` around a value. A value stored with the quote characters still in it is a broken path. Do not put quotes around ordinary values.

```ini
cookies_filename = cookies.sqlite
base_path = ~/.app/gemini-wilgat
window_title = Gemini (wilgat)
```

Defaults live only as macros in `config.h`. `config_init()` and the writer of the default file both read those macros.

### 2.1 Implementation notes (this project)

`PROJECT_ID` is `gemini-wilgat`. The file is `~/.app/gemini-wilgat/config.conf`.

```ini
[core]
window_title = Gemini (wilgat)
default_width = 1200
default_height = 800

[ui]
show_navigation_buttons = true
show_user_agent_label = true
navigation_buttons_enabled = true

[storage]
base_path = ~/.app/gemini-wilgat
cookies_enabled = true
cookies_filename = cookies.sqlite
settings_filename = settings.sqlite

[output]
quiet = false
json = false
debug = false

[bookmarks]
enabled = true

[scripts]
enabled = true
path = ~/.app/gemini-wilgat/userscripts

[parameters]
shutdown_seconds = 0
bookmark_index = 0
```

### 2.2 Verification

| TP-ID | Pass |
|-------|------|
| TP-CONFIG-CREATE | A first launch with no config file creates `~/.app/gemini-wilgat/config.conf` and the window still opens |

## 3. Protection rule

Future editors must not hard-code a login home directory in this file, accept a quoted filename as a real filename, or move the default macros out of `config.h`.

## 4. Related artifacts

| Artifact | Role |
|----------|------|
| `docs/requirements/index.md` | Registry |
| `docs/requirements/requirement-cpp-parameters.md` | Flags that override this file |
| `docs/requirements/requirement-cpp-window.md` | What the flags change on screen |
| `docs/requirements/requirement-cpp-ui-menu.md` | What `[bookmarks] enabled` shows |
| `docs/requirements/requirement-cpp-webui-bookmark.md` | Bookmark URLs; `bookmark_index` is written and not read |
| `docs/requirements/requirement-cpp-lua-script-commands.md` | Reads `[scripts] path`. `[scripts] enabled` does not hide that menu |
| `docs/reviews/test-plan.md` | TP-CONFIG-CREATE |
| `src/main/cpp/config.h` | Default macros |
| `src/main/cpp/config.cpp` | Load and create |

**Last updated**: 2026-10-04
**Owner**: Configuration
