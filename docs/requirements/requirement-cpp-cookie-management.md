**file**: docs/requirements/requirement-cpp-cookie-management.md
**Status**: Active
**Updated**: 2026-10-02
**Philosophy**: CIAO v2.10.2 / CIAO-Lite (Caution • Intentional • Anti-fragile • Over-engineered / Over-protect)

## 1. Purpose

This file owns the persistent cookie file so a Gemini login can survive a restart. WebKit attaches it during setup. The setup order is the window requirement.

### 1.1 Human-facing

**In one sentence:** You stay logged in because cookies are written to `~/.app/gemini-wilgat/cookies/cookies.sqlite` before the first page loads.

| Box | Meaning | Example |
|-----|---------|---------|
| You / this login | The person who signed in to Gemini | You close the window and open it again |
| The other role | `cookies.cpp` | It creates the directory and the SQLite file |
| Not this file | The settings key/value table | `requirement-cpp-database.md` |

| Includes | Excludes |
|----------|----------|
| Cookie path, create-if-missing, debug count | User-agent rows |

| Surface | What you open | What for |
|---------|---------------|----------|
| `~/.app/gemini-wilgat/cookies/cookies.sqlite` | Cookie jar | Session cookies |
| The terminal with `DEBUG=1` | Debug log | Cookie count |

| You do… | What it means | What you type |
|---------|---------------|---------------|
| Confirm the jar exists | TP-COOKIE-PERSIST | see `docs/reviews/test-plan.md` |

## 2. Core rules

- Directory: `~/.app/` plus `PROJECT_ID` plus `/cookies/`.
- File name: `cookies.sqlite`.
- `add_cookie()` runs from `setup_webkit_webview()` before any page load.
- The directory is created with `g_mkdir_with_parents` when missing. Failure is logged. A missing `HOME` is reported and does not crash before that log.
- When `DEBUG=1`, startup prints the cookie count through `ciao_debug`.

### 2.1 Implementation notes (this project)

The file is `~/.app/gemini-wilgat/cookies/cookies.sqlite`.

### 2.2 Verification

| TP-ID | Pass |
|-------|------|
| TP-COOKIE-PERSIST | After a restart the file is still at that path and `DEBUG=1` prints a cookie count |

## 3. Protection rule

Future editors must not load a page before `add_cookie()`, or hard-code a project name other than `PROJECT_ID` into the cookie path.

## 4. Related artifacts

| Artifact | Role |
|----------|------|
| `docs/requirements/index.md` | Registry |
| `docs/requirements/requirement-cpp-window.md` | When cookies are attached |
| `docs/reviews/test-plan.md` | TP-COOKIE-PERSIST |
| `src/main/cpp/cookies.cpp` | Implementation |

**Last updated**: 2026-10-02
**Owner**: Cookies
