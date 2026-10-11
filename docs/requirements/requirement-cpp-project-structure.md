**file**: docs/requirements/requirement-cpp-project-structure.md
**Status**: Active
**Updated**: 2026-10-04
**Philosophy**: CIAO v2.10.2 / CIAO-Lite (Caution • Intentional • Anti-fragile • Over-engineered / Over-protect)

## 1. Purpose

This file owns the two directories people confuse: the source tree, and the runtime data directory under the home folder. Product identity comes from `project.h`.

### 1.1 Human-facing

**In one sentence:** Source stays in this repository, and cookies and settings stay in `~/.app/VideoTools/`, which is a different folder.

| Box | Meaning | Example |
|-----|---------|---------|
| You / this login | The person who builds or runs the app | You open `src/main/cpp/` |
| The other role | The runtime folder | It appears on first launch |
| Not this file | Menu behavior, and the log directory | `requirement-cpp-window.md` and `requirement-cpp-logger-folder.md` |

| Includes | Excludes |
|----------|----------|
| Source layout and runtime filenames | SQL column lists, and the log directory |

| Surface | What you open | What for |
|---------|---------------|----------|
| Repository root | Source and `./build.sh` | Build |
| `~/.app/VideoTools/` | Runtime data | Config, settings, cookies, userscripts |

| You do… | What it means | What you type |
|---------|---------------|---------------|
| Find the binary | Simple build writes it under `target/exe/debug/` | `./build.sh build` |

## 2. Core rules

Source repository (milestone 1.0):

```text
VideoTools/
├── README.md
├── build.sh
├── project.sh
├── project.ini
├── meson.build
├── src/main/cpp/          # C++ sources
├── target/              # build output, not source
└── docs/requirements/   # this registry
```

Runtime directory, created by the program, not by git:

```text
~/.app/<PROJECT_ID>/
├── config.conf
├── settings.sqlite
├── cookies/cookies.sqlite
└── userscripts/*.lua
```

Filenames:

| File | Name |
|------|------|
| Config | `config.conf` |
| Settings | `settings.sqlite` |
| Cookies | `cookies/cookies.sqlite` |
| Userscripts | `*.lua` in `userscripts/` |

Paths in C++ come from `PROJECT_ID` and the config getters. A literal `~/.app/gmail-wilgat` or a literal `~/.app/r2webpad` in source is a defect.

### 2.1 Implementation notes (this project)

`PROJECT_ID` is `VideoTools`. The runtime root is `~/.app/VideoTools/`. The g++ binary is `target/exe/debug/VideoTools`.

## 3. Protection rule

Future editors must not commit `target/` or describe the runtime folder as a child of the git tree.

## 4. Related artifacts

| Artifact | Role |
|----------|------|
| `docs/requirements/index.md` | Registry |
| `docs/requirements/requirement-cpp-configuration.md` | `config.conf` rules |
| `src/main/cpp/project.h` | `PROJECT_ID` |
| `docs/requirements/requirement-cpp-logger-folder.md` | Log directory, Deferred |
| `docs/requirements/requirement-cpp-lua-script-commands.md` | Menu and verb for `userscripts/` |

**Last updated**: 2026-10-04
**Owner**: Project structure
