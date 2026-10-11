**file**: docs/requirements/requirement-cpp-skills-installation-gnu-make.md
**Status**: Deferred
**Updated**: 2026-10-02
**Philosophy**: CIAO v2.10.2 / CIAO-Lite (Caution • Intentional • Anti-fragile • Over-engineered / Over-protect)

## 1. Purpose

GNU Make and Autotools are not a build of gemini-wilgat. `./build.sh` does not implement `make` or `gnumake`. The live compile paths are `requirement-cpp-skills-installation-basic.md` and `requirement-cpp-skills-installation-meson.md`.

### 1.1 Human-facing

**In one sentence:** You do not configure this window with Autotools, because that verb is not in `build.sh`.

| Box | Meaning | Example |
|-----|---------|---------|
| You / this login | The person who compiles | You use `./build.sh build` |
| The other role | A future Make skill | It is not in this milestone |
| Not this file | The live GCC build | `requirement-cpp-skills-installation-basic.md` |

| Includes | Excludes |
|----------|----------|
| The statement that Make and Autotools are deferred | `./build.sh make`, `autoreconf`, or a binary named `r2webpad` |

| Surface | What you open | What for |
|---------|---------------|----------|
| `./build.sh` | Live verbs | `install`, `clean`, `build` |
| `docs/requirements/index.md` | Status | This row is Deferred |

| You do… | What it means | What you type |
|---------|---------------|---------------|
| Compile now | Use the primary build | `./build.sh build` |

## 2. Core rules

- Do not document `./target/exe/debug/r2webpad` as this product's binary.
- Do not document `./build.sh make`, `./build.sh gnumake`, `autoreconf`, or `./configure` as a current command.
- This file becomes Active only in the same change that adds the verb to the `build.sh` case statement and names the binary `target/exe/debug/gemini-wilgat`.

## 3. Protection rule

Future editors must not mark this file Active while `./build.sh` has no `make` verb.

## 4. Related artifacts

| Artifact | Role |
|----------|------|
| `docs/requirements/index.md` | Registry |
| `docs/requirements/requirement-cpp-skills-installation-basic.md` | Primary build |
| `build.sh` | Live verbs |

**Last updated**: 2026-10-02
**Owner**: Installation skills
