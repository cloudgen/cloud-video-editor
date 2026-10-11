**file**: docs/requirements/requirement-cpp-dependencies.md
**Status**: Active
**Updated**: 2026-10-11
**Philosophy**: CIAO v2.10.2 / CIAO-Lite (Caution • Intentional • Anti-fragile • Over-engineered / Over-protect)

## 1. Purpose

This file owns the libraries the gemini-wilgat C++ program compiles and links against, and the apt packages that provide them. `./build.sh install` is the command that requests the apt packages. `./build.sh build` and `meson.build` both request the same link set. How those commands compile stays in the build requirements. How sol2 runs Lua stays in `requirement-cpp-lua-script.md`.

### 1.1 Human-facing

**In one sentence:** You install libwebkit2gtk, libsqlite3, liblua, GTK, GLib, and epoxy, and both builds link those same libraries. libmpv is headers plus a runtime load, not a link.

| Box | Meaning | Example |
|-----|---------|---------|
| You / this login | The person who prepares the machine | You run `./build.sh install` |
| The other role | The link line | It asks pkg-config for WebKit, SQLite, and Lua |
| Not this file | Menu behavior, or which Lua libraries a script may call | `requirement-cpp-window.md` and `requirement-cpp-lua-script.md` |

| Includes | Excludes |
|----------|----------|
| The link modules, the apt packages, sol2 headers, and pthread | A second package list inside the class file |

| Surface | What you open | What for |
|---------|---------------|----------|
| `./build.sh` | `install_deps` and the `pkg-config` call | Apt packages and the link set |
| `meson.build` | `dependency()` entries | The same link set |

| You do… | What it means | What you type |
|---------|---------------|---------------|
| Install the libraries | Apt packages below, then sol2 headers when `<sol/sol.hpp>` is missing. `libmpv-dev` supplies headers. The link line does not pass `-lmpv` | `./build.sh install` |

## 2. Core rules

The link set is exactly these modules. `./build.sh build` passes the pkg-config rows to `pkg-config --cflags --libs`. `meson.build` declares each required one with `dependency()`, `required: true`.

| Library | pkg-config / Meson name | Apt package |
|---------|-------------------------|-------------|
| GTK 3 | `gtk+-3.0` | `libgtk-3-dev` |
| WebKitGTK 4.1 (`libwebkit2gtk-4.1`) | `webkit2gtk-4.1` | `libwebkit2gtk-4.1-dev` |
| GLib | `glib-2.0` | `libglib2.0-dev` |
| GObject | `gobject-2.0` | `libglib2.0-dev` |
| GIO | `gio-2.0` | `libglib2.0-dev` |
| SQLite (`libsqlite3`) | `sqlite3` | `libsqlite3-dev` |
| Lua 5.3 (`liblua5.3`) | `lua5.3` | `liblua5.3-dev` and `lua5.3` |
| epoxy | `epoxy` | `libepoxy-dev` |
| POSIX threads | Meson name `threads`; the simple link line adds `-pthread` | comes with `build-essential` |
| libdl | the simple link line adds `-ldl` | comes with the C library |
| sol2 | header `<sol/sol.hpp>` | no apt package; headers under `/usr/local/include/sol` |

libmpv is not in that link set. `build.sh` and `meson.build` may pass `-I` or compile-only mpv cflags so `mpv/client.h` resolves. They must not pass `--libs mpv` or `-lmpv`. `./build.sh install` may install `libmpv-dev` for the headers. The picture window loads `libmpv.so.2` at runtime. A missing `mpv/client.h` fails the compile with the sentence that names `MPV_INCLUDE_DIR` and says not to link `-lmpv`.

`./build.sh install` also installs `build-essential`, `pkg-config`, and `git`. `git` is there so the install step can clone sol2. The clone steps stay in `requirement-cpp-skills-installation-basic.md`. The sol2 API stays in `requirement-cpp-lua-script.md`.

The second build needs `meson` and `ninja-build` on the developer machine. `./build.sh install` does not install those two packages.

`project.ini` `REQUIRED_PACKAGES` also names `gstreamer1.0-plugins-good` and `gstreamer1.0-plugins-ugly`. `AVOIDED_PACKAGES` names `gstreamer1.0-plugins-bad`. Those three packages are absent from the link line, and `./build.sh install` does not install them. GStreamer environment variables stay in `requirement-cpp-window.md`.

`project.sh` `REQUIRED_LIBS` names `gtk+-3.0`, `webkit2gtk-4.1`, `sqlite3`, and `lua5.3`. `./build.sh` sources `project.sh` for paths. The `pkg-config` call inside `build()` uses the full link set in the table above.

Package versions are the distro packages for these names. This file has no version pins and no lockfile.

### 2.1 Implementation notes (this project)

Simple build, the `pkg-config` arguments:

```text
gtk+-3.0 webkit2gtk-4.1 glib-2.0 gobject-2.0 gio-2.0 sqlite3 lua5.3
```

The link line appends `-pthread` and `-ldl`. It does not append `-lmpv`.

`meson.build` repeats those modules, adds `dependency('epoxy')` and `dependency('threads')`, and takes mpv compile arguments only when the mpv dependency is found. Its C++ include directory is `/usr/local/include` so `<sol/sol.hpp>` resolves.

### 2.2 Why this requirement exists

- **CIAO Principle 5** (https://github.com/cloudgen/ciao): one dependency list. Both builds consume the table in this file.
- **CIAO Principle 1**: a missing `libwebkit2gtk`, `libsqlite3`, or `liblua` fails at configure or link time, on the module name in the table.

## 3. Design principles

- **Caution**: a library joins the link set only when product code includes it.
- **Intentional**: a new linked library is added to the table, to the `pkg-config` call, and to `meson.build` in the same change.
- **Anti-fragile**: GLib, GObject, and GIO stay named even though one apt package provides all three.
- **Over-protect**: sol2 stays a header dependency. It is not replaced with a second Lua binding.

## 4. Verification

| TP-ID | Pass |
|-------|------|
| TP-CPP-DEPENDENCIES | `pkg-config --exists` succeeds for `gtk+-3.0`, `webkit2gtk-4.1`, `glib-2.0`, `gobject-2.0`, `gio-2.0`, `sqlite3`, `lua5.3`, and `epoxy`. `build.sh` and `meson.build` both request that set, pass mpv include flags only, and do not pass `-lmpv` |

Map: `docs/reviews/test-plan.md`. Status stays todo until that check is run.

## 5. Protection rule

Future editors must not drop `libwebkit2gtk-4.1-dev`, `libsqlite3-dev`, `liblua5.3-dev`, or `libepoxy-dev` from this table, link a WebKit or Lua version other than 4.1 and 5.3, add `-lmpv` to the link line, or copy the package list back into `requirement-class-software-dev.md`.

## 6. Related artifacts

| Artifact | Role |
|----------|------|
| `docs/requirements/index.md` | Registry |
| `docs/requirements/requirement-class-software-dev.md` | Class; points here |
| `docs/requirements/requirement-cpp-skills-installation-basic.md` | `install` and the sol2 clone |
| `docs/requirements/requirement-cpp-skills-installation-meson.md` | `dependency()` call sites |
| `docs/requirements/requirement-cpp-lua-script.md` | sol2 API |
| `docs/requirements/requirement-cpp-window.md` | GStreamer environment variables |
| `docs/reviews/test-plan.md` | TP-CPP-DEPENDENCIES |
| `build.sh` | Apt list and `pkg-config` call |
| `meson.build` | `dependency()` entries |
| `project.ini` | `REQUIRED_PACKAGES`, including the GStreamer plugin names |
| `project.sh` | Shorter `REQUIRED_LIBS` |

**Last updated**: 2026-10-11
**Owner**: C++ dependencies
