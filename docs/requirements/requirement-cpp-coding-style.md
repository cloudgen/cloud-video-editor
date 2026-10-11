# C++ coding style

**file**: docs/requirements/requirement-cpp-coding-style.md
**Status**: Active
**Updated**: 2026-10-04

## 1. Purpose

This file is where portable C++ lessons are specialized for gemini-wilgat. Without this requirement, those lessons arrive raw. Class law points here and does not copy these rules.

### 1.1 Human-facing

**In one sentence:** You change C++ in this repository by following the linkage and logging rules in this file, and the window behavior stays in the window requirement.

| Box | Meaning | Example |
|-----|---------|---------|
| You / this login | The person editing source | You add a callback in a `.cpp` file |
| The other role | The compiler and linker | They reject a `static` function that another file calls |
| Not this file | Which page opens, or which flags exist | `requirement-cpp-window.md` and `requirement-cpp-parameters.md` |

| Includes | Excludes |
|----------|----------|
| `static`, `extern "C"`, comment shape, `ciao_*` only | Menu contents and SQLite schemas |

| Surface | What you open | What for |
|---------|---------------|----------|
| `src/main/cpp/*.cpp` | Product source | The rules below |
| `src/main/cpp/output.h` | Logging API | The only console output |

| You do… | What it means | What you type |
|---------|---------------|---------------|
| Share a callback across files | Leave it non-static and guard the header with `extern "C"` | edit the `.h` the `.cpp` files include |

## 2. Core rules

gemini-wilgat follows **CIAO-Lite**:
- **C** - Caution
- **I** - Intentional
- **A** - Anti-fragile
- **O** - Over-protect

**Priority**: Safety, clarity of intention, and long-term maintainability always have higher priority than simplicity or minimalism.

> **🚨 CRITICAL ZERO-TOLERANCE RULE (ANTI-HARDCODING)**  
> **NO hard-coded URLs, paths, project names, filenames, or service-specific strings are allowed anywhere in the source code.**  
> All such values **MUST** come from `project.h` (Single Source of Truth) or configuration files.  
> Violating this rule breaks forking, Milestone 2.0 requirements, and CIAO-Lite principles.

## Cross-File Function Visibility Rules (Mandatory)

- **`static` functions are file-local only.**  
  A function marked `static` can **only** be called from within the same `.cpp` file.

- **Never** mark a function `static` if it needs to be called from another source file.

### When to Avoid `static` (Strict Rule)

Use **non-static** (external linkage) functions in these cases:
- GTK signal callbacks that are connected using `g_signal_connect()` from a different `.cpp` file.
- Any function that is declared in a `.h` header and used across multiple source files.

**Examples (Must remain non-static)**:
- `on_go_button_clicked()` in `url_bar_go_button.cpp`
- `on_uri_changed()` and `on_load_changed()` in `url_bar_page.cpp` / `url_bar_user_agent.cpp`
- These callbacks are connected from `url_bar_user_agent.cpp` or other modules.

**Rule**:
> Future AI or developers **MUST NOT** add the `static` keyword to any function that is used across multiple `.cpp` files via `g_signal_connect()` or direct calls from other modules. Doing so will cause linker errors or "undefined reference" failures.

This rule exists because the GTK signal system relies on external linkage for callbacks connected across files.

## C linkage on shared headers (`extern "C"`) - Sacred Rule

Every product translation unit is C++. Shared headers still wrap their public functions in `extern "C"`.

**Incident Summary (May 2026)**  
When `userscript_manager.cpp` was the only C++ unit, the linker could not match it with the C units (`output.c`, `config.c`, `path_utils.c`, and the rest). C and C++ mangle names differently. Missing guards made those functions look as if they had disappeared.

**Resolution**  
Shared headers keep conditional `extern "C"` guards. Those guards give the public functions C linkage, which is what `g_signal_connect` and the other GLib callbacks call.

**Correct Pattern (Mandatory)**

```cpp
// userscript_manager.h (example)
#ifndef USERSCRIPT_MANAGER_H
#define USERSCRIPT_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

void initialize_userscript_manager(WebKitWebView *web_view);
void userscript_on_url_changed(const gchar *uri);

#ifdef __cplusplus
}
#endif

#endif // USERSCRIPT_MANAGER_H
```

## Comment Style
- Every function must have:
  - Purpose
  - Parameters
  - Dependencies
  - Why This Design
  - CIAO Principles Applied
  - Protection Rule (if applicable)
- Use clear section dividers: `──────────────────────────────────────────────`

## Output & Logging
- ONLY use `ciao_info()`, `ciao_warn()`, `ciao_error()`, `ciao_debug()`
- Never use `printf`, `fprintf`, `g_print` directly (except inside ciao_*)
- Support `--quiet`, `--json`, and `DEBUG=1`
- C++ important values use `ciao_debug` and are listed in `requirement-cpp-debug-mode.md`

A project log content type is one kind of message the dated log stores. The five types are the catalog. A message that matches a type is stored. A message that matches none is not required in the dated file. The stored line is `requirement-cpp-logger.md`.

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

A direct `g_warning` outside the logger is still **gnome-debug** and must be stored. New product code does not add another `g_warning`. The logger row owns the store.

## UI view for an app log

The UI view for an app log is the HTML table the main web view shows for one log file. The behavior is `requirement-cpp-log-commands.md` §2.8. `log show` is not this view. It prints the file bytes unchanged.

| Piece | Rule |
|-------|------|
| Document | One HTML document in the main web view. One table. |
| Header | `Time`, `Pid`, `Level`, `Source`, `Message`. Background `#1f4e79`, white bold text. |
| Stripes | Odd body rows `#ffffff`. Even body rows `#e7eef6`. |
| Hover | A body row under the pointer uses `#ffe08a`. |
| Cells | Displayed text omits `[` `]` `<` `>` `(` `)` `{` `}`. |
| Source | The stored component without `@`. A trailing `.cpp` or `.c` is omitted. The file still stores that extension. |
| File | The view reads the file. It does not write it. |

## Configuration (Milestone 2.0+)
- **NO hardcoded URLs, paths, project names, filenames, or behaviors in source code** — this is a **hard rule**.
- All configurable values **MUST** come from config files (`config.conf`), command-line parameters, or `project.h` (Single Source of Truth).

## Skills System
- Skills must be treated as configurable composite objects
- Core should not contain service-specific logic

## File Organization Rules
- `src/main/cpp/core/` → General container code
- `src/main/cpp/skills/` → Service-specific skills
- Headers and implementation clearly separated
- Single Source of Truth (`project.h` for basic metadata only)

---

**Last Updated**: 2026-10-06  
**Owner**: CIAO-Lite Style Guide
