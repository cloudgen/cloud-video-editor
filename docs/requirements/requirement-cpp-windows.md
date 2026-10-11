**file**: docs/requirements/requirement-cpp-windows.md
**Status**: Active (Version 1.0.0)
**Area**: windows
**Key**: `requirement-cpp-windows`
**Philosophy**: CIAO v2.10.2 / CIAO-Lite (Caution • Intentional • Anti-fragile • Over-engineered / Over-protect)

## 1. Purpose

This file owns the Windows contract for VideoTools: how a child process starts, how paths are joined, and how `ffmpeg` is found on Windows cmd. The picture window on Windows is the same window as `requirement-cpp-gui.md` when that build has GTK, libmpv, and a display. This file does not claim that the current checkout was compiled on Windows.

### 1.1 Human-facing

**In one sentence:** On Windows cmd you run VideoTools as yourself, with `ffmpeg` and `ffprobe` already on `PATH`.

| Box | Meaning | Example |
|-----|---------|---------|
| You / this login | The person in Command Prompt | You run `VideoTools join --first a.mp4 --second b.mp4` |
| The other role | The process host | It starts `ffmpeg` with `CreateProcessW` and no shell |
| Not this file | Widget layout and the WebKit shell | `requirement-cpp-gui.md` and `requirement-cpp-window.md` |

| Includes | Excludes |
|----------|----------|
| `CreateProcessW`, backslash joins, `PATH` split on `;`, `ffmpeg.exe` lookup, wide-character directory listing | `sudo`, `winget`, `apt`, a shell around `ffmpeg`, a claim that this checkout was built with a Windows compiler |

| Surface | What you open | What for |
|---------|---------------|----------|
| Windows cmd | `VideoTools` | Join, edit, list, help, about, version |
| A Windows build that has GTK, libmpv, and a display | The picture window | The same edit and join pages |

| You do… | What it means | What you type |
|---------|---------------|---------------|
| Join two files | The program finds `ffmpeg` on `PATH` and starts it without a shell | `VideoTools join --first a.mp4 --second b.mp4` |
| Name a file with backslashes | The base name is the part after the last slash or backslash | `VideoTools edit --file C:\clips\bar.mp4 --start 0 --end 1` |

## 2. Core rules

### 2.1 Process start

`host_spawn` is the only child launcher used by join, edit, and ffprobe.

| Rule | Contract |
|------|----------|
| Windows start | `CreateProcessW`. The command line is one quoted wide string. Arguments are UTF-8 converted with `MultiByteToWideChar` |
| Flags | `CREATE_NO_WINDOW`. Handles are inherited only for the capture pipe and `NUL`. No `cmd.exe` |
| Capture | One of standard output or standard error. The other side is `NUL`. Standard input is `NUL` |
| Empty argv | Return 127 and `empty command` |
| Pipe or create failure | Return 127 and `pipe failed` or `CreateProcessW failed` |
| POSIX start | `fork`, `execvp`, and `waitpid`. A failed exec returns 127 |
| Return | The child exit code, or 127 when the child never ran |

The binary does not call `system`, `popen`, or a shell to run `ffmpeg`.

### 2.2 Paths and PATH

| Helper | Contract |
|--------|----------|
| Base name and parent | Split on `/` and on `\` |
| Join | On `_WIN32` the separator is `\`. Elsewhere it is `/` |
| Real path | `_fullpath` on Windows. `realpath` elsewhere. Empty string on failure |
| Executable lookup | Split `PATH` on `;` on Windows and on `:` elsewhere. On Windows accept the name and the name plus `.exe` via `_access`. Elsewhere require `X_OK` |
| Temp directory | `GetTempPathA` on Windows. `TMPDIR` or `/tmp` elsewhere |
| Temp file | `GetTempFileNameA` plus `MoveFileA` when a suffix is required. Elsewhere `mkstemp` |
| Directory listing | `FindFirstFileW` on Windows, with UTF-8 conversion. `opendir` elsewhere |

A join or edit output name with no `/` and no `\` is prefixed with the current folder before `ffmpeg` runs. `ffmpeg` otherwise writes relative to the current folder. An edit `--file` that already contains `/` or `\`, or that begins with a drive letter and `:`, is not prefixed with `--folder`.

### 2.3 Picture window on Windows

The route in `requirement-cpp-gui.md` is unchanged. When the Windows build has GTK 3, a display, and both terminal sides, a bare start opens that window. Playback loads `libmpv-2.dll`, then `mpv-2.dll`. The link line still does not pass `-lmpv`. GL procedure addresses on Windows come from `wglGetProcAddress` first. When that returns NULL, the same name is resolved with `GetProcAddress` on `opengl32.dll`. The system-log check appends `\` after `_fullpath` when the directory has no trailing separator.

The verified compiler for this checkout is `g++` on Linux. Windows sources are in the tree. This checkout was not compiled on Windows, so a Windows picture is a contract, not a measured result.

### 2.4 Implementation notes (this project)

| Item | Value |
|------|--------|
| Host | `src/main/cpp/video_host.cpp` |
| Callers | `run_process`, `probe_duration`, and `make_temp` in `src/main/cpp/video_domain.cpp` |
| Tools | `ffmpeg` and `ffprobe` on `PATH`. On Windows the lookup also accepts `ffmpeg.exe` and `ffprobe.exe` |
| Binary name | `VideoTools`. A Windows build may be `VideoTools.exe` |
| Privilege | Normal user. The binary does not install packages |

### 2.5 Why this requirement exists

- **CIAO Principle 5** (https://github.com/cloudgen/ciao): one process host for POSIX and Windows, so join and edit do not grow a second launcher.
- **CIAO Principle 10**: Windows cmd stays a normal-user command line. The binary does not elevate.

## Under command line for normal user only

Windows cmd is a normal-user command line. On that detect, admin privilege and a dedicated system user stay unused. The binary does not call `sudo`, wrap `apt` or `winget`, create an account, or recommend `sudo curl | sh`. Git Bash is not a reason to call Termux `pkg`. The person installs `ffmpeg` and `ffprobe` themselves and puts them on `PATH`.

**This requirement:** process start, path joins, and `PATH` lookup add no elevation and no package install.

## 3. Design principles

- **Caution**: create failure returns 127 with a named reason. It does not fall through to a shell.
- **Intentional**: `CreateProcessW` is the Windows launcher. `fork` and `execvp` stay the POSIX launcher.
- **Anti-fragile**: both `/` and `\` split. A Windows-style path can be named on a POSIX build for the base name and the parent.
- **Over-protect**: `CREATE_NO_WINDOW` and no shell. The child cannot be a command string the shell re-parses.

## 4. Protection rule

Future editors must not start `ffmpeg` through `cmd.exe`, `system`, or `popen`, must not make the binary install `ffmpeg`, and must not report a Windows compile of this checkout unless that compile was actually run.

## 5. Related artifacts

| Artifact | Role |
|----------|------|
| `docs/requirements/index.md` | Registry |
| `docs/requirements/requirement-cpp-gui.md` | Picture window, including the Windows library names |
| `docs/requirements/requirement-domain-videotools.md` | Join and edit jobs that call the host |
| `docs/requirements/requirement-cpp-dependencies.md` | Libraries. libmpv stays off the link line |
| `src/main/cpp/video_host.cpp` | Process and path host |
| `tests/test_video_host.cpp` | Base name, parent, and join checks |

**Last updated**: 2026-10-11
**Owner**: VideoTools Windows host
