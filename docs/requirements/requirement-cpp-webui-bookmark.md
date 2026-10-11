**file**: docs/requirements/requirement-cpp-webui-bookmark.md
**Status**: Active
**Updated**: 2026-10-02
**Philosophy**: CIAO v2.10.2 / CIAO-Lite (Caution • Intentional • Anti-fragile • Over-engineered / Over-protect)

## 1. Purpose

This file owns the WebUI bookmark list: the one menu item, the URL it loads, and which index `--bookmark` opens at startup. Whether the Bookmark item is shown stays in `requirement-cpp-ui-menu.md`. The flag names stay in `requirement-cpp-parameters.md`. `[bookmarks] enabled` stays in `requirement-cpp-configuration.md`.

### 1.1 Human-facing

**In one sentence:** You choose Bookmark → Start Page, or you launch with no bookmark index, and the window opens `https://gemini.google.com`.

| Box | Meaning | Example |
|-----|---------|---------|
| You / this login | The person who picks a saved page | You choose Start Page |
| The other role | `add_bookmark_menu_app()` | It builds the one-item submenu |
| Not this file | Where Bookmark sits in the File menu | `requirement-cpp-ui-menu.md` |

| Includes | Excludes |
|----------|----------|
| The label, the URL, the click, and the startup index | AutoClick URLs and the Follow script |

| Surface | What you open | What for |
|---------|---------------|----------|
| File → Bookmark | The submenu | Start Page |
| `src/main/cpp/ui_window_app.cpp` | Startup load | The index chosen by `--bookmark` |

| You do… | What it means | What you type |
|---------|---------------|---------------|
| Open the only bookmark | The view loads `START_PAGE` | choose File → Bookmark → Start Page |
| Start on that same page | Index 0 is the default | launch the binary |

## 2. Core rules

The bookmark list has one row. The URL is the `START_PAGE` macro in `project.h`. `PROFILE_PAGE` is not a row.

| Index | Label | URL |
|-------|-------|-----|
| 0 | Start Page | `START_PAGE` (`https://gemini.google.com`) |

A comment in `output.cpp` says index 1 is Contacts. That comment is stale. There is no Contacts bookmark.

`add_bookmark_menu_app()` is the only function that builds the submenu. A NULL `url_entry` returns NULL. The item label is `Bookmark`. The child label is `Start Page`.

Activating Start Page calls `on_menu_item_clicked()`, which calls `load_page()` with that URL and writes the URL into the URL entry. `on_menu_bookmark_clicked()` in `menu_bookmark.cpp` is not connected. It is not the live click path.

### 2.1 Startup index

`--bookmark=N` and `BOOKMARK=N` set `ciao_bookmark_index`. The parser clamps a negative value to 0. `[parameters] bookmark_index` is written into `config.conf` and is not read.

`create_main_window()` loads the page with `load_page_with_login_check()`.

| Index | What loads |
|-------|------------|
| 0, or no flag | `START_PAGE`. The lookup branch is skipped |
| 1 | The startup slot is `NULL`. A warning is logged. The page stays `START_PAGE` |
| 3 or more | The `< 3` check fails. A warning is logged. The page stays `START_PAGE` |
| 2 | The check is `ciao_bookmark_index < 3` against an array of two slots, so this reads past the `NULL` sentinel. Index 2 is not a bookmark |

The startup array is a second copy of the list, not the menu array:

```c
const char *bookmark_urls[] = {
    START_PAGE,  /* index 0 */
    NULL
};
```

`[bookmarks] enabled` hides the menu item. It does not change the startup URL.

### 2.2 Why this requirement exists

- **CIAO Principle 5** (https://github.com/cloudgen/ciao): one bookmark list. The menu row and the startup slot name the same `START_PAGE`.
- **CIAO Principle 1**: an index that is not 0 stays on Start Page and says so in the log, except the past-the-end read at index 2, which this file forbids.

## 3. Design principles

- **Caution**: a bookmark URL is a `project.h` macro.
- **Intentional**: a new bookmark is a new row in the table, an `ADD_BOOKMARK` in `add_bookmark_menu_app()`, and a slot before the `NULL` in `create_main_window()`, in the same change. The startup bound matches the array, so it stops at the sentinel.
- **Anti-fragile**: index 1 and any index of 3 or more keep Start Page.
- **Over-protect**: `PROFILE_PAGE` is not added to fill a second slot.

## 4. Verification

| TP-ID | Pass |
|-------|------|
| TP-WEBUI-BOOKMARK | The Bookmark submenu has one item, Start Page, and it loads `https://gemini.google.com`. Startup with no flag loads that URL. `--bookmark=1` logs a warning and still loads Start Page |

Map: `docs/reviews/test-plan.md`. Status stays todo until that check is run.

## 5. Protection rule

Future editors must not add `PROFILE_PAGE` as a bookmark, hard-code a bookmark URL outside `project.h`, connect `on_menu_bookmark_clicked()` without naming it the live path in this file, or leave the startup check as `< 3` on a two-slot array.

## 6. Related artifacts

| Artifact | Role |
|----------|------|
| `docs/requirements/index.md` | Registry |
| `docs/requirements/requirement-cpp-ui-menu.md` | Bookmark's place in the File menu |
| `docs/requirements/requirement-cpp-parameters.md` | `--bookmark=N` and `BOOKMARK=N` |
| `docs/requirements/requirement-cpp-configuration.md` | `[bookmarks] enabled`; `bookmark_index` is written and not read |
| `docs/requirements/requirement-cpp-window.md` | `START_PAGE` and `PROFILE_PAGE` |
| `docs/reviews/test-plan.md` | TP-WEBUI-BOOKMARK |
| `src/main/cpp/menu_bookmark_app.cpp` | Submenu and the live click |
| `src/main/cpp/menu_bookmark.cpp` | Unconnected click handler |
| `src/main/cpp/ui_window_app.cpp` | Startup array |
| `src/main/cpp/project.h` | `START_PAGE` |
| `src/main/cpp/output.cpp` | `ciao_bookmark_index` |

**Last updated**: 2026-10-02
**Owner**: WebUI bookmark
