**file**: docs/requirements/requirement-cpp-webui-auto-follow.md
**Status**: Active
**Updated**: 2026-10-02
**Philosophy**: CIAO v2.10.2 / CIAO-Lite (Caution • Intentional • Anti-fragile • Over-engineered / Over-protect)

## 1. Purpose

This file owns WebUI auto-follow: the JavaScript that clicks a Follow button, and the allow-list that decides when that script is injected. The AutoClick menu item and its navigation list stay in `requirement-cpp-ui-menu.md`. Lua `inject_javascript` stays in `requirement-cpp-lua-script.md`.

### 1.1 Human-facing

**In one sentence:** A Gemini page clicks Follow only when its URL is on the auto-follow allow-list, and that list is empty, so browsing does not click Follow.

| Box | Meaning | Example |
|-----|---------|---------|
| You / this login | The person looking at a Gemini page | You open the start page |
| The other role | `inject_auto_follow_script()` | It runs the Follow script in the WebKit view |
| Not this file | The AutoClick menu item | `requirement-cpp-ui-menu.md` |

| Includes | Excludes |
|----------|----------|
| The Follow script, the allow-list, and the 800 ms delay | A second click script in a userscript requirement |

| Surface | What you open | What for |
|---------|---------------|----------|
| `src/main/cpp/auto_follow.cpp` | The script | The click and the retry |
| `src/main/cpp/url_bar_page.cpp` | `on_uri_changed` | The allow-list check |

| You do… | What it means | What you type |
|---------|---------------|---------------|
| Open a page | The URI is compared to `auto_follow_urls` | launch the binary and load Gemini |
| Match the allow-list | The script is injected 800 ms later | an exact URL from that array |

## 2. Core rules

- `inject_auto_follow_script()` in `src/main/cpp/auto_follow.cpp` is the only function that evaluates the Follow script. A NULL view, a non-object, or a widget that is not a `WebKitWebView` returns without evaluating it.
- The script looks for `button[aria-label="Follow"]`. When that button is present it calls `click()` once. When it is absent the script waits 500 ms and tries again, up to 20 attempts, then stops.
- The allow-list is `auto_follow_urls` in `src/main/cpp/url_bar_page.cpp`. The array holds only the terminating `NULL`. An editor must not invent bot URLs in that array.
- `on_uri_changed()` runs the check on `notify::uri`. An exact `g_strcmp0` match schedules `delayed_auto_follow` after 800 ms, and that callback calls `inject_auto_follow_script()` once. A prefix or substring overlap logs a partial match and does not schedule injection. A missing URI does not schedule injection.
- With the current array, every URI is a miss. Opening `START_PAGE` does not inject the script.
- `autoclick_bot_urls` is a different array, owned by `requirement-cpp-ui-menu.md`. Adding a URL there does not add it here.
- `on_load_finished()` and `delayed_inject()` in `menu_bookmark_app.cpp` wait 100 ms and then inject, but `on_load_finished` is not connected to `load-changed`. That path does not run. `ui_window_app.cpp` declares `inject_auto_follow_script` and does not call it.
- The URI decision logs through `ciao_info` and `ciao_debug` with the prefix `[AUTO-FOLLOW]`. The JavaScript callback in `auto_follow.cpp` currently prints `[AUTO_FOLLOW]` with `g_print`. Which print function is allowed stays in `requirement-cpp-output.md`.

### 2.1 Implementation notes (this project)

Allow-list:

```c
static const char *auto_follow_urls[] = {
    NULL
};
```

The script body is the string `AUTO_FOLLOW_SCRIPT`. The selector and the retry counts are the ones in section 2.

### 2.2 Why this requirement exists

- **CIAO Principle 5** (https://github.com/cloudgen/ciao): one injection entry. Follow clicks go through `inject_auto_follow_script()`.
- **CIAO Principle 1**: an empty allow-list is a visible miss, not a silent click on every Gemini page.

## 3. Design principles

- **Caution**: injection happens only after an exact allow-list hit.
- **Intentional**: the selector, the 20 attempts, the 500 ms retry, and the 800 ms delay are the numbers. A change to any of them is a change to this file in the same edit.
- **Anti-fragile**: a missing Follow button stops after 20 attempts.
- **Over-protect**: the unconnected load-finished handler is not a second live trigger.

## 4. Verification

| TP-ID | Pass |
|-------|------|
| TP-WEBUI-AUTO-FOLLOW | `auto_follow_urls` is only the `NULL` sentinel, so a URI change does not schedule injection. The script queries `button[aria-label="Follow"]`, clicks once, and retries at 500 ms for at most 20 attempts |

Map: `docs/reviews/test-plan.md`. Status stays todo until that check is run.

## 5. Protection rule

Future editors must not inject the Follow script on every page, add a URL to `auto_follow_urls` without adding it to the table in this file, or treat `on_load_finished()` as a live trigger while it stays disconnected.

## 6. Related artifacts

| Artifact | Role |
|----------|------|
| `docs/requirements/index.md` | Registry |
| `docs/requirements/requirement-cpp-ui-menu.md` | AutoClick navigation list |
| `docs/requirements/requirement-cpp-lua-script.md` | Lua `inject_javascript` |
| `docs/requirements/requirement-cpp-output.md` | `ciao_*` versus `g_print` |
| `docs/requirements/requirement-cpp-window.md` | WebKit view that receives the script |
| `docs/reviews/test-plan.md` | TP-WEBUI-AUTO-FOLLOW |
| `src/main/cpp/auto_follow.cpp` | Script and injection entry |
| `src/main/cpp/auto_follow.h` | Declaration |
| `src/main/cpp/url_bar_page.cpp` | Allow-list and the 800 ms delay |
| `src/main/cpp/menu_bookmark_app.cpp` | Unconnected 100 ms path |

**Last updated**: 2026-10-02
**Owner**: WebUI auto-follow
