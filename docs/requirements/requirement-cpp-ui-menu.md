**file**: docs/requirements/requirement-cpp-ui-menu.md
**Status**: Active
**Updated**: 2026-10-04
**Philosophy**: CIAO v2.10.2 / CIAO-Lite (Caution • Intentional • Anti-fragile • Over-engineered / Over-protect)

## 1. Purpose

This file owns the gemini-wilgat menu bar: the File and About menus, their items, and what activating an item does. `build_main_menu_bar()` in `src/main/cpp/ui_menu_app.cpp` is the only function that builds that bar. Where the bar sits in the window stays in `requirement-cpp-window.md`. The `[bookmarks] enabled` flag stays in `requirement-cpp-configuration.md`. The `user_agent` row stays in `requirement-cpp-database.md`.

### 1.1 Human-facing

**In one sentence:** You open File or About and get AutoClick, one bookmark, five user agents, Lua Script, Log, Exit, and Show Version.

| Box | Meaning | Example |
|-----|---------|---------|
| You / this login | The person at the keyboard | You choose Start Page or Exit |
| The other role | `build_main_menu_bar()` | It assembles File and About |
| Not this file | WebKit setup order, or the INI parser | `requirement-cpp-window.md` and `requirement-cpp-configuration.md` |

| Includes | Excludes |
|----------|----------|
| File and About items, the Bookmark item's place, the five user-agent items, Lua Script, Log, Exit, Show Version, and the unattached About dialog | Log actions (`requirement-cpp-log-commands.md`), Lua Script actions (`requirement-cpp-lua-script-commands.md`), and the bookmark URL list (`requirement-cpp-webui-bookmark.md`) |

| Surface | What you open | What for |
|---------|---------------|----------|
| The menu bar | File and About | Navigation, user agent, version |
| `src/main/cpp/ui_menu_app.cpp` | The builder | The only assembly point |

| You do… | What it means | What you type |
|---------|---------------|---------------|
| Open Start Page | The bookmark item loads `START_PAGE` | choose File → Bookmark → Start Page |
| Leave | Exit ends the GTK main loop | choose File → Exit |
| Open a log | Log lists numbered files, the folder, and Clear log | choose File → Log |
| Open a lua script | Lua Script lists the stored scripts, the folder, and New lua script | choose File → Lua Script |

## 2. Core rules

`ui_window_app.cpp` packs the widget returned by `build_main_menu_bar()`. That function returns NULL when `url_entry`, `web_view`, or `parent_window` is NULL. The top-level items are File, then About.

### 2.1 File menu

`config_get_bookmarks_enabled()` follows `[bookmarks] enabled` from `requirement-cpp-configuration.md`. The default of that flag is true.

When bookmarks are enabled, File contains, in order:

| Item | Behavior |
|------|----------|
| AutoClick all bots | Starts the AutoClick sequence |
| A separator | Visual only |
| Bookmark | Submenu owned by `requirement-cpp-webui-bookmark.md` |
| User Agent | Submenu in section 2.3 |
| Lua Script | Submenu in section 2.8. Actions are `requirement-cpp-lua-script-commands.md` |
| Log | Submenu in section 2.7. Actions are `requirement-cpp-log-commands.md` |
| Exit | Section 2.4 |

When bookmarks are disabled, AutoClick, the separator, and Bookmark are omitted. User Agent, Lua Script, Log, and Exit stay.

### 2.2 Bookmark submenu

The Bookmark item is present when bookmarks are enabled. Its label, its URL, the click, and the startup index are owned by `requirement-cpp-webui-bookmark.md`.

### 2.3 User Agent submenu

The submenu label is `User Agent`. It has exactly these five items, in this order. The strings live only in `project.h`.

| Menu label | Macro |
|------------|-------|
| Webview (Default) | `UA_WEBVIEW` |
| Google Chrome | `UA_CHROME` |
| Mozilla Firefox | `UA_FIREFOX` |
| Apple Safari | `UA_SAFARI` |
| Microsoft Edge | `UA_EDGE` |

Activating an item sets that string on the WebKit view, copies it to the user-agent label, and saves it with `save_user_agent_to_settings()`. The stored key and the next-launch apply step stay in `requirement-cpp-database.md` and `requirement-cpp-window.md`. The default applied when nothing is stored is `DEFAULT_USER_AGENT`, which is `UA_WEBVIEW`.

The debug line in `add_user_agent_menu()` says `default: Google Chrome`. That text is stale. The default item is Webview (Default).

### 2.4 Exit

The Exit item calls `on_exit_menu_item_activate()`, which logs through `ciao_info` and then calls `gtk_main_quit()`. Closing the window also quits, from the `destroy` handler in `ui_window_app.cpp`. The shutdown timer stays in `requirement-cpp-parameters.md`.

### 2.5 AutoClick

The URL list in `menu_bookmark_app.cpp` is `autoclick_bot_urls`. The array holds only the terminating `NULL`. Activating AutoClick must not invent bot URLs. No page is visited until a URL is added to that list in a later change. Clicking Follow on a loaded page is owned by `requirement-cpp-webui-auto-follow.md`.

### 2.6 About menu

About contains one item.

| Item | Behavior |
|------|----------|
| Show Version | Opens the version dialog |

`on_show_version_activate()` requires a parent window. The dialog title is `About` plus `PROJECT_NAME`. The body shows `PROJECT_NAME`, the three version macros, and `PROJECT_DESCRIPTION`.

`on_show_about_activate()` in `dialog_about.cpp` builds a `GtkAboutDialog` (program name, version, comments, copyright, MIT license, website, authors). `build_main_menu_bar()` does not connect it. It is not a menu item. Comments shown by that function must be `PROJECT_DESCRIPTION` from `project.h`. They must name this product.

### 2.7 Log

The Log item is always present, including when bookmarks are disabled. Its label is `Log`. It sits before Exit. The submenu labels and the actions are owned by `requirement-cpp-log-commands.md`. Choosing a numbered file loads the HTML log table into the main web view. That sheet is §2.8 of the log-commands requirement. `log show` still prints the file bytes.

### 2.8 Lua Script

The Lua Script item is always present, including when bookmarks are disabled and when `[scripts] enabled` is false. Its label is `Lua Script`. It sits after User Agent and before Log. The submenu labels, the filename dialog, and the actions are owned by `requirement-cpp-lua-script-commands.md`.

### 2.9 Why this requirement exists

- **CIAO Principle 5** (https://github.com/cloudgen/ciao): one menu builder. Items are added in `build_main_menu_bar()` and the functions it calls.
- **CIAO Principle 1**: a bookmark or a user agent is a row in the tables above, with its URL or macro named beside the label.

## 3. Design principles

- **Caution**: a NULL `url_entry`, web view, or parent window returns no menu bar.
- **Intentional**: File order and the five user-agent labels are the lists. A new item is added to the matching table in the same change as the `gtk_menu_shell_append` call.
- **Anti-fragile**: a failed Bookmark, User Agent, Lua Script, or Log submenu is a warning, and Exit still appears.
- **Over-protect**: user-agent strings stay in `project.h`. Bookmark URLs are owned by `requirement-cpp-webui-bookmark.md`.

## 4. Verification

| TP-ID | Pass |
|-------|------|
| TP-UI-MENU | With bookmarks enabled, File shows AutoClick all bots, Bookmark, User Agent, Lua Script, Log, and Exit, and About shows Show Version. User Agent has five items. Lua Script sits after User Agent and before Log. Log sits before Exit |
| TP-UA-RESTORE | The menu choice is the user agent applied on the next launch |

Map: `docs/reviews/test-plan.md`. Status stays todo until that check is run.

## 5. Protection rule

Future editors must not add a second menu builder, hard-code a user-agent outside `project.h`, restate the bookmark URL list in this file, or attach the About dialog without adding it to the About table in the same change.

## 6. Related artifacts

| Artifact | Role |
|----------|------|
| `docs/requirements/index.md` | Registry |
| `docs/requirements/requirement-cpp-window.md` | Where the bar sits, and WebKit apply order |
| `docs/requirements/requirement-cpp-configuration.md` | `[bookmarks] enabled` |
| `docs/requirements/requirement-cpp-webui-bookmark.md` | Bookmark list and startup index |
| `docs/requirements/requirement-cpp-parameters.md` | `--bookmark=N` |
| `docs/requirements/requirement-cpp-database.md` | `user_agent` row |
| `docs/reviews/test-plan.md` | TP-UI-MENU, TP-UA-RESTORE |
| `src/main/cpp/ui_menu_app.cpp` | Menu bar |
| `src/main/cpp/menu_bookmark_app.cpp` | Bookmark item and AutoClick list |
| `docs/requirements/requirement-cpp-webui-auto-follow.md` | Follow click script |
| `src/main/cpp/menu_user_agent.cpp` | User Agent submenu |
| `src/main/cpp/menu_exit.cpp` | Exit |
| `src/main/cpp/menu_log.cpp` | Log item |
| `src/main/cpp/menu_lua_script.cpp` | Lua Script item |
| `docs/requirements/requirement-cpp-log-commands.md` | Log actions |
| `docs/requirements/requirement-cpp-lua-script-commands.md` | Lua Script actions |
| `src/main/cpp/dialog_version.cpp` | Show Version |
| `src/main/cpp/dialog_about.cpp` | Unattached About dialog |
| `src/main/cpp/project.h` | `START_PAGE` and the user-agent strings |

**Last updated**: 2026-10-04
**Owner**: UI menu
