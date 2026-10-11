**file**: docs/requirements/requirement-cpp-milestones.md
**Status**: Active
**Updated**: 2026-10-04
**Philosophy**: CIAO v2.10.2 / CIAO-Lite (Caution • Intentional • Anti-fragile • Over-engineered / Over-protect)

## 1. Purpose

This file is the milestone list for gemini-wilgat. It is not a copy of the settings-database requirement.

### 1.1 Human-facing

**In one sentence:** Milestone 1.0 is the window you can build today; later milestones are named here and are not in the binary yet.

| Box | Meaning | Example |
|-----|---------|---------|
| You / this login | The person deciding what is in scope | You treat a deferred row as not built |
| The other role | The registry | Deferred requirements match the later milestones |
| Not this file | The schema of `settings.sqlite` | `requirement-cpp-database.md` |

| Includes | Excludes |
|----------|----------|
| What 1.0 includes and what is later | SQL and menu tables |

| Surface | What you open | What for |
|---------|---------------|----------|
| `docs/requirements/index.md` | Status column | Active versus Deferred |
| The gemini-wilgat window | Milestone 1.0 | What you can run |

| You do… | What it means | What you type |
|---------|---------------|---------------|
| Build milestone 1.0 | Simple GCC build of the current sources | `./build.sh build` |

## 2. Milestones

### Milestone 1.0 — current window

In the tree now:

- GTK window, URL bar, user-agent label, WebKit view.
- Start page `https://gemini.google.com`.
- Menu bar owned by `requirement-cpp-ui-menu.md`. The chosen user agent is persisted in `settings.sqlite`.
- WebUI auto-follow owned by `requirement-cpp-webui-auto-follow.md`. The allow-list is empty.
- WebUI bookmark list owned by `requirement-cpp-webui-bookmark.md`. One item, Start Page.
- Cookie file and `config.conf` under `~/.app/gemini-wilgat/`.
- `ciao_*` output and the flags in the parameters requirement.
- Lua userscripts from `~/.app/gemini-wilgat/userscripts/`.
- File → Lua Script and the `luascript` verb, owned by `requirement-cpp-lua-script-commands.md`.
- Show Version and the unattached About dialog, owned by `requirement-cpp-ui-menu.md`.
- GStreamer environment and auto-allow permission requests.
- `./build.sh` verbs `install`, `clean`, and `build`. Binary `target/exe/debug/gemini-wilgat`.
- Meson builds the same `src/main/cpp/*.cpp` sources.

### Milestone 2.0 — config-driven leftovers

Still to do, not claimed done:

- Remove product names that are still typed in comments or dialogs instead of `project.h`.
- Drive the bookmark list from `config.conf` if a second bookmark is added.

### Milestone 3.0 — skill loader

Not in the binary. `requirement-cpp-skills.md` is Deferred. Userscripts remain the extension mechanism.

## 3. Protection rule

Future editors must not paste the settings schema or the cookie rules into this file.

## 4. Related artifacts

| Artifact | Role |
|----------|------|
| `docs/requirements/index.md` | Registry |
| `docs/requirements/requirement-cpp-window.md` | Milestone 1.0 behavior |
| `docs/requirements/requirement-cpp-database.md` | Settings file, not this list |

**Last updated**: 2026-10-04
**Owner**: Milestones
