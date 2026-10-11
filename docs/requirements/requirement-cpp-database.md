**file**: docs/requirements/requirement-cpp-database.md
**Status**: Active
**Updated**: 2026-10-02
**Philosophy**: CIAO v2.10.2 / CIAO-Lite (Caution • Intentional • Anti-fragile • Over-engineered / Over-protect)

## 1. Purpose

This file owns `settings.sqlite`: the engine, the path, the table, and the debug dump. Cookie storage is `requirement-cpp-cookie-management.md`. Milestone dates are `requirement-cpp-milestones.md`.

### 1.1 Human-facing

**In one sentence:** The program stores your user-agent choice in `~/.app/gemini-wilgat/settings.sqlite`, and `DEBUG=1` prints every row at startup.

| Box | Meaning | Example |
|-----|---------|---------|
| You / this login | The person whose choice is saved | You pick Firefox in the menu |
| The other role | `settings.cpp` | It creates the table and reads `user_agent` |
| Not this file | The menu labels | `requirement-cpp-window.md` |

| Includes | Excludes |
|----------|----------|
| Path, schema, parameterized access, startup dump | Cookie file layout |

| Surface | What you open | What for |
|---------|---------------|----------|
| `~/.app/gemini-wilgat/settings.sqlite` | Settings database | Key/value rows |
| The terminal with `DEBUG=1` | Debug log | The dump |

| You do… | What it means | What you type |
|---------|---------------|---------------|
| Inspect saved settings | Every `key = value` row is printed at startup | `DEBUG=1` plus the binary |

## 2. Core rules

SQLite is the only database engine for settings. Access goes through `settings.cpp`. Failures use `ciao_error`. Queries that take outside text are parameterized.

Path: `~/.app/` plus `PROJECT_ID` plus `/settings.sqlite`. The filename is `settings.sqlite`.

```sql
CREATE TABLE IF NOT EXISTS settings (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
);
```

The user-agent row uses key `user_agent`. The window requirement owns when that row is read and written.

When `DEBUG=1`, startup calls `ciao_debug` with every row in this table.

### 2.1 Implementation notes (this project)

`PROJECT_ID` is `gemini-wilgat`. The file is `~/.app/gemini-wilgat/settings.sqlite`.

## 3. Protection rule

Future editors must not open this database from a file other than `settings.cpp`, or copy this schema into the milestones requirement.

## 4. Related artifacts

| Artifact | Role |
|----------|------|
| `docs/requirements/index.md` | Registry |
| `docs/requirements/requirement-cpp-window.md` | Who reads `user_agent` |
| `docs/requirements/requirement-cpp-cookie-management.md` | The other SQLite file |
| `src/main/cpp/settings.cpp` | Implementation |

**Last updated**: 2026-10-02
**Owner**: Settings database
