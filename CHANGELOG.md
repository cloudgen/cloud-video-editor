# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-10-11

### Added
- cloud-video-editor domain on the gemini-wilgat bootstrap: `join`, `edit`, `list-videos`, `list-mp4`, `help`, `about`, `version`, `tui`, and a numbered menu.
- Picture window for edit and join. Playback is libmpv inside the panes. Encode stays FFmpeg. The binary loads `libmpv.so.2` at runtime and does not link `-lmpv`.
- Windows process host: `CreateProcessW`, backslash joins, and `ffmpeg.exe` on `PATH`. The verified compile of this checkout is `g++` on Linux.
- `g++ -std=c++17` remains the primary compile. The binary is `target/exe/debug/cloud-video-editor`. Ant and Gradle write that same path. Meson writes `builddir/cloud-video-editor`.
- Join uses FFmpeg stream copy, then a re-encode. Edit cuts, changes length, and can append a boomerang or a repeat. Duration comes from `ffprobe`.

### Changed
- Product identity is cloud-video-editor `1.0.0` (`project.h`, `project.ini`). A bare start on a terminal with a display opens the picture window. With no display, or when standard output is not a terminal, that start shows the menu. With no terminal on standard input, it prints help.
- README pictures show the current edit page after a file is chosen, and the join page before any file is chosen.
- `--gemini` opens the inherited wilgat shell. `log`, `luascript`, and `logcheck` still open that shell. `--window` is an error.
- The shell runtime id is `cloud-video-editor`. Cookies, userscripts, and logs for that shell use `~/.app/cloud-video-editor/`.

### Fixed
- Closing the window while Run is in progress unrealizes the picture panes before the players are released.
- The picture thread does not wait on the player client while a render context is created or freed.
- At most one pane is audible. A video-only MP4 omits audio filters, and a join re-encode pads a silent partner instead of failing.
- `log`, `luascript`, and `logcheck` are shell verbs only as the first positional word. A flag value with those names still runs edit or join. `--window` stays an error beside them.
- Windows edit paths that already contain `\` or a drive letter are not prefixed with `--folder`. The system-log check uses `\` after `_fullpath`. OpenGL 1.1 procedure addresses fall back to `opengl32.dll`.

Entries below are the gemini-wilgat bootstrap history this checkout was copied from.

## [1.0.6] - 2026-04-18

### Added
- RECOMMENDATION.md

## [1.0.5] - 2026-04-18

### Added
- Persistent User-Agent settings using `settings.sqlite` (stored alongside cookies).
- User-Agent selected at startup now correctly displays in the UI label (previously showed default Chrome until changed).
- `webkit_setup.c/h` module — Single Point of Truth for all WebKitWebContext, cookie, and User-Agent initialization.
- `ui_window_gemini.c/h` and `ui_menu_gemini.c/h` modules for better separation of concerns.
- Strong CIAO defensive protection comments on critical functions (`create_main_window()`, `build_main_menu_bar()`, etc.).
- `PROJECT_DESCRIPTION` constant in `project.h` (Single Source of Truth).

### Changed
- `main.c` is now pure orchestration (very clean and short).
- `add_url_bar_user_agent()` now accepts an `initial_ua` parameter to respect persisted settings.
- Enhanced protection rules to preserve GitHub-specific UI modules when forking to other projects.
- Updated version dialog to use centralized `PROJECT_DESCRIPTION`.

### Fixed
- UI label under URL bar now shows correct persisted User-Agent on application startup.
- Duplicate loading of User-Agent settings at startup.

---

## [1.0.3] - 2026-04-18

### Added
- Full User-Agent spoofing menu (Chrome, Firefox, Safari, Edge) with Chrome as default.
- Persistent cookie storage using SQLite.
- Advanced output system with `--quiet`, `--json`, and `DEBUG=1` support.
- Generic bookmark system + GitHub-specific bookmarks.
- Real-time URL bar synchronization (`notify::uri` + `load-changed` fallback).
- Enter key support in URL bar.

### Changed
- Migrated to strong CIAO Defensive Programming style across the entire codebase.
- All User-Agent strings centralized in `project.h`.

---

## [1.0.0] - 2026-04-14

### Added
- Initial release of lightweight native GitHub wilgat using GTK3 + WebKit2GTK.
- Basic URL bar and navigation.
- Defensive coding foundation (CIAO principles).

---

## How to Maintain This File

When releasing a new version:
1. Move `[Unreleased]` content into a new version header with today's date.
2. Add a fresh `[Unreleased]` section at the top.
3. Use the standard categories: **Added**, **Changed**, **Fixed**, etc.

---

**Project**: [cloudgen/gemini-wilgat](https://gemini.com/cloudgen/gemini-wilgat)  
**Philosophy**: Built with strict CIAO Defensive Programming Principles

---

Made with ❤️ and extreme caution.