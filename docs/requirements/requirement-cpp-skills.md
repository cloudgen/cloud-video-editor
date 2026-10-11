**file**: docs/requirements/requirement-cpp-skills.md
**Status**: Deferred
**Updated**: 2026-10-02
**Philosophy**: CIAO v2.10.2 / CIAO-Lite (Caution • Intentional • Anti-fragile • Over-engineered / Over-protect)

## 1. Purpose

A pluggable skill manager (enable, disable, and order service modules from config) is not in the gemini-wilgat binary. The live way to extend the window is Lua userscripts, owned by `requirement-cpp-window.md`. The C++ host uses sol2, owned by `requirement-cpp-lua-script.md`.

### 1.1 Human-facing

**In one sentence:** You extend the current window with a Lua file, not with a skill package, because the skill loader is not built.

| Box | Meaning | Example |
|-----|---------|---------|
| You / this login | The person who wants a new page behavior | You add a `.lua` userscript |
| The other role | A future skill loader | It does not exist in this milestone |
| Not this file | Userscript rules | `requirement-cpp-window.md` |

| Includes | Excludes |
|----------|----------|
| The statement that the loader is deferred | A requirement to ship Gmail, Poe, or YouTube modules now |

| Surface | What you open | What for |
|---------|---------------|----------|
| `~/.app/gemini-wilgat/userscripts/` | Current extension point | Lua hooks |
| `docs/requirements/index.md` | Status | This row is Deferred |

| You do… | What it means | What you type |
|---------|---------------|---------------|
| Add behavior now | Follow the userscript section of the window requirement | place a `.lua` file in the userscripts directory |

## 2. Core rules

- Milestone 1.0 does not load `[skills]` or `[skill.*]` config sections.
- Do not treat translate, Gmail, or Poe samples as current product law.
- When a skill loader is implemented, this file becomes Active in the same change and the window requirement keeps userscripts unless that change moves them.

## 3. Protection rule

Future editors must not mark this file Active while `config.h` has no skill-module table and no loader runs from `main()`.

## 4. Related artifacts

| Artifact | Role |
|----------|------|
| `docs/requirements/index.md` | Registry |
| `docs/requirements/requirement-cpp-window.md` | Live extension mechanism |
| `docs/requirements/requirement-cpp-lua-script.md` | sol2 host |
| `docs/requirements/requirement-cpp-milestones.md` | Milestone 3.0 |

**Last updated**: 2026-10-02
**Owner**: Skills
