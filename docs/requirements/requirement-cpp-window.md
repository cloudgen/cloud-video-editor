**file**: docs/requirements/requirement-cpp-window.md
**Status**: Active
**Updated**: 2026-10-11
**Philosophy**: CIAO v2.10.2 / CIAO-Lite (Caution • Intentional • Anti-fragile • Over-engineered / Over-protect)

## 1. Purpose

This is the single owner of the live gemini-wilgat window: what page opens, how WebKit is created, how userscripts load, and how audio and permission requests are handled. Other requirement files point here. They do not restate this behavior. The menu bar is owned by `requirement-cpp-ui-menu.md`. The sol2 binding that runs those scripts is owned by `requirement-cpp-lua-script.md`. The video picture window is a different surface and is owned by `requirement-cpp-gui.md`. `--gemini` opens this shell. `--window` does not.

### 1.1 Human-facing

**In one sentence:** You launch gemini-wilgat and it opens Gemini in a GTK window, with optional Lua userscripts.

| Box | Meaning | Example |
|-----|---------|---------|
| You / this login | The person at the keyboard | You launch the window |
| The other role | The WebKit window | It loads `https://gemini.google.com` |
| Not this file | The File and About items, and the video picture window | `requirement-cpp-ui-menu.md` and `requirement-cpp-gui.md` |

| Includes | Excludes |
|----------|----------|
| Start page, WebKit setup order, userscripts, GStreamer environment, permission requests | The menu items (`requirement-cpp-ui-menu.md`) |

| Surface | What you open | What for |
|---------|---------------|----------|
| The gemini-wilgat window | The WebKit view | The start page |
| `~/.app/cloud-video-editor/userscripts/*.lua` | Userscript directory | Page hooks |

| You do… | What it means | What you type |
|---------|---------------|---------------|
| Open the start page | The default page is Gemini | launch the binary |
| Open a userscript | Every `*.lua` file in the userscripts directory is loaded | place `gemini-log.lua` in that directory |

## 2. Core rules

### 2.1 Identity of the window

- The runtime id is `PROJECT_ID` in `src/main/cpp/project.h`. For this product that value is `cloud-video-editor`.
- The display name is `PROJECT_NAME`: `cloud-video-editor`.
- Cookies, userscripts, and logs for this shell live under `~/.app/cloud-video-editor/` because those paths follow `PROJECT_ID`. Root or sudo logs use `/var/cloud-video-editor/log`.
- The start page is `START_PAGE`: `https://gemini.google.com`.
- `PROFILE_PAGE` is defined as `START_PAGE` plus `/settings/profile`. It is not the start page. The bookmark list must not add it; that rule is owned by `requirement-cpp-webui-bookmark.md`.

### 2.2 Layout

Top to bottom:

1. Menu bar.
2. URL bar (back, forward, reload, entry, Go) when `show_navigation_buttons` is true.
3. Read-only user-agent label when `show_user_agent_label` is true.
4. The WebKit view, which takes the remaining space.

The URL entry follows `notify::uri`. Enter does the same thing as Go. Page loads go through `load_page()` or `load_page_with_login_check()` in `src/main/cpp/load_page_ogin_check.cpp`.

The items inside the menu bar are owned by `requirement-cpp-ui-menu.md`.

### 2.3 Startup URL

The page opened at startup is the bookmark selected by `--bookmark` / `BOOKMARK`. The list and the fallback are owned by `requirement-cpp-webui-bookmark.md`. With the default index, the page is `START_PAGE`.

### 2.4 User agent applied to the view

The five menu items are owned by `requirement-cpp-ui-menu.md`. The strings live only in `project.h`. `setup_webkit_webview()` applies the stored `user_agent` before any page load. The default stored value is `DEFAULT_USER_AGENT` (`UA_WEBVIEW`).

### 2.5 AutoClick

The AutoClick item and its URL list are owned by `requirement-cpp-ui-menu.md`.

### 2.6 WebKit setup order

`setup_webkit_webview()` in `webkit_setup.cpp` is the only place that creates the `WebKitWebContext` and the `WebKitWebView`. Order:

1. Create the context.
2. Attach persistent cookies (`add_cookie`).
3. Load `user_agent` from settings. If none is stored, save `DEFAULT_USER_AGENT`.
4. Create the view with autoplay allowed.
5. Apply the user agent and media settings, including `enable-webaudio` and `enable-webaudio-realtime-thread`.
6. Connect `permission-request`.
7. Call `initialize_userscript_manager()`.
8. When `DEBUG=1`, report the settings rows.

### 2.7 Permission requests

`on_permission_request()` allows the request and returns true. It logs through `ciao_debug`. It does not prompt.

### 2.8 Show Version and About

The About menu and the unattached About dialog are owned by `requirement-cpp-ui-menu.md`.

### 2.9 Process environment for audio

`main()` sets these environment variables before `gtk_init()` and before WebKit setup:

| Variable | Value |
|----------|-------|
| `AUDIODEV` | `default` |
| `GST_PLUGIN_FEATURE_RANK` | `pulsesink:MAX,autoaudiosink:MAX,pipewiresink:NONE,alsasink:NONE` |
| `GST_AUDIO_RESAMPLER_QUALITY` | `10` |
| `PULSE_LATENCY_MSEC` | `15` |
| `GST_PULSE_NO_LATENCY` | `1` |
| `WEBKIT_GST_AUDIO_RESAMPLING` | `1` |

`login_detector_app.cpp` only keeps `inject_tts_keeper()`. Login clicking belongs in a userscript, not in that file.

### 2.10 Userscripts

Directory: `~/.app/` plus `PROJECT_ID` plus `/userscripts`. For this product that is `~/.app/cloud-video-editor/userscripts`. The directory is created at startup with mode `0700` when missing. Showing that folder, and viewing, editing, or creating a file there, is `requirement-cpp-lua-script-commands.md`.

Every `*.lua` file in that directory is loaded in alphabetical order. Header lines `-- @match` and `-- @priority` are read. Higher priority runs first. The default priority is 50. A script with no `@match` runs on every URL change. When `DEBUG=1` is on, the loader prints the values owned by `requirement-cpp-debug-mode.md`.

The C++ host is sol2, owned by `requirement-cpp-lua-script.md`. That file owns which Lua libraries are opened.

| Function | Behavior |
|----------|----------|
| `log(message)` | `ciao_info` |
| `inject_javascript(js_code)` | Runs in the current view |
| `get_current_url()` | Current view URI, or empty |
| `register_on_url_changed(fn)` | Called when the URI changes |

Filename grammar: prefix none, one stem, suffix `.lua`, placed by the person, no sequence number.

Sample basename: `gemini-log.lua`

```lua
-- @name         Gemini log
-- @description  Log Gemini page loads
-- @match        https://gemini.google.com/*
-- @priority     50

function on_url_changed(url)
    if string.match(url, "gemini%.google%.com") then
        log("gemini url " .. url)
    end
end

register_on_url_changed(on_url_changed)
```

### 2.11 Verification

| TP-ID | Pass |
|-------|------|
| TP-USERSCRIPT-LOAD | `gemini-log.lua` is loaded and a matching URL calls `on_url_changed` |

Map: `docs/reviews/test-plan.md`.

## 3. Protection rule

Future editors must not add a second window requirement, hard-code a user-agent outside `project.h`, or put a Gmail or GitHub URL in this file as the start page.

## 4. Related artifacts

| Artifact | Role |
|----------|------|
| `docs/requirements/index.md` | Registry |
| `docs/requirements/requirement-cpp-configuration.md` | Window size and feature flags |
| `docs/requirements/requirement-cpp-database.md` | `user_agent` row storage |
| `docs/requirements/requirement-cpp-cookie-management.md` | Cookie file |
| `docs/reviews/test-plan.md` | TP-USERSCRIPT-LOAD |
| `src/main/cpp/project.h` | Identity and user-agent strings |
| `src/main/cpp/webkit_setup.cpp` | Setup order |
| `docs/requirements/requirement-cpp-ui-menu.md` | Menu bar |
| `src/main/cpp/ui_menu_app.cpp` | Menu builder |
| `src/main/cpp/userscript_manager.cpp` | Lua loader |
| `docs/requirements/requirement-cpp-lua-script.md` | sol2 host |
| `docs/requirements/requirement-cpp-lua-script-commands.md` | Folder, view, edit, and new |
| `docs/requirements/requirement-cpp-debug-mode.md` | Debug values from that loader |

**Last updated**: 2026-10-04
**Owner**: Window
