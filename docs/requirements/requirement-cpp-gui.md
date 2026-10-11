**file**: docs/requirements/requirement-cpp-gui.md
**Status**: Active (Version 1.0.0)
**Area**: window
**Key**: `requirement-cpp-gui`
**Philosophy**: CIAO v2.10.2 / CIAO-Lite (Caution • Intentional • Anti-fragile • Over-engineered / Over-protect)

## 1. Purpose

This file owns the VideoTools picture window: which start opens it, the edit and join controls, playback inside the panes, and the encode result the window shows. The gemini-wilgat WebKit shell is a different window. That shell stays in `requirement-cpp-window.md`. `--gemini` opens that shell. `--window` is an error and opens neither window.

### 1.1 Human-facing

**In one sentence:** On a terminal that has a display, VideoTools with no words opens the video window.

| Box | Meaning | Example |
|-----|---------|---------|
| You / this login | The person at the keyboard | You start VideoTools with no words |
| The other role | The picture window | It shows the edit page and plays the file you pick |
| Not this file | The WebKit shell, and the numbered menu | `requirement-cpp-window.md` and `requirement-domain-videotools.md` |

| Includes | Excludes |
|----------|----------|
| Edit and join pages, in-pane playback, encode result labels, system-log, language, and self pages | The WebKit start page, pip package buttons, a translated label catalog |

| Surface | What you open | What for |
|---------|---------------|----------|
| `VideoTools` | Picture window | Edit or join when a display and both terminal sides are present |
| `VideoTools --gemini` | Inherited shell | The WebKit window, not this file |
| `VideoTools tui` | Numbered menu | Text menu, not this window |

| You do… | What it means | What you type |
|---------|---------------|---------------|
| Open the picture window | No words, a display, and both standard input and standard output are terminals | `VideoTools` |
| Stay on the menu | No display, or standard output is not a terminal | `VideoTools` |
| Refuse a bad flag | `--window` prints an error and does not open a window | `VideoTools --window` |

## 2. Core rules

### 2.1 Which start opens the window

The route is decided before GTK and before the WebKit environment block.

| Start | Result |
|-------|--------|
| No words, standard input is a terminal, standard output is a terminal, and `WAYLAND_DISPLAY` or `DISPLAY` is non-empty | Picture window |
| The same start with only `--quiet` | Picture window. `parse_args` ignores `--quiet` |
| No words, and standard input is not a terminal | Help, exit 0. No window |
| No words, and standard output is not a terminal, or neither display variable is set | Numbered menu. No window |
| `tui` and standard input is a terminal | Numbered menu |
| `tui` and standard input is not a terminal | `ERROR: tui needs a terminal.` Exit 1 |
| `--gemini` alone | Inherited shell. Not this window |
| `--gemini` with a verb, a video flag, or a bad flag | `ERROR: --gemini cannot be combined with another mode.` Exit 1. No GTK |
| `--window` | `ERROR: --window is not the video window. A bare start opens the video window. --gemini opens the inherited shell.` Exit 1 |
| A known verb, a video flag, a complete edit, or any other bad argv | Headless. No window |
| `--quiet`, `--shutdown=`, and `--bookmark=` | Shell-only. They do not by themselves block the window or `--gemini` |

`WAYLAND_SOCKET` is not an open condition. The program does not set `GDK_BACKEND`.

A refused display prints one stderr line and exits 1. It does not fall through to the menu.

```text
ERROR: the display was refused. The next step is VideoTools tui.
```

The video path calls `gtk_disable_setlocale()` and then `gtk_init_check()`. It does not call `gtk_init`. It does not call `ciao_output_init`. It may call `ciao_debug_from_env()`.

### 2.2 Chrome

The title is `VideoTools` plus the version. The default size is 1280×800. Edit is the first page. The group strip is `join` and `edit`, with the open group highlighted, and no row numbers. The header ends with system-log, language, self-management, and Exit. `help` and `tui` are not window buttons. The path label is `Path: ` plus the absolute current folder.

English labels stay English. The language page says `This tree has no language catalog. Labels stay English.` The self page shows VideoTools, the version, the project description, and the binary path. It has no pip buttons.

### 2.3 Edit page

The left side has the MP4 row, Select file, a player, the elapsed clock, the scale, the total, Start, Pause, Stop, a jump entry, Go, Mark start, Mark end, and editable Start and End. Percent is 20–200 and starts at 100. The tail is None, Boomerang, or Repeat. None starts selected.

The right side shows the output path, Run, the result label, and a Generated player with Play and Stop.

Select file opens `GtkFileChooserDialog` titled `Select MP4`, local files only, current folder, Cancel and Open. The chooser is not a native portal dialog. A path that is not a regular file, or that contains `://`, is rejected before playback. Edit accepts an MP4 suffix. The reject sentence is `Edit accepts an MP4 file.`

Choosing a file sets Start to `00:00.00` and End to the ffprobe duration. The clock is `MM:SS.CC`. Run stays off until start is before end. The output label uses two spaces: `Output  {path}`.

### 2.4 Join page

Video 1 is above Video 2. Each row has a path, Select file, and a player with Play and Stop. The chooser title is `Select video`. Suffixes are `.mp4` `.mov` `.mkv` `.avi` `.m4v`. A wrong suffix says `That file is not a join video.` The same path on both rows says `Choose two different files.`

The output name is editable. Empty becomes `{stem1} + {stem2}.mp4`. The folder label uses two spaces: `Write to  {folder}` when a name is set, and `Write to  the current folder` when it is empty. A third Generated player sits under Run.

### 2.5 Playback

Each pane has its own `mpv_create` and render context. Drawing uses `GtkGLArea` and `mpv_render_context_render` into that area's framebuffer. The program does not spawn the `mpv` binary and does not embed with `wid`.

The library is loaded at runtime: `libmpv.so.2`, then `libmpv.so`. On Windows the names are `libmpv-2.dll`, then `mpv-2.dll`. The link line does not pass `-lmpv`. A missing library replaces the pane with `Playback is unavailable (libmpv).` Encode still runs. An OpenGL failure uses `Playback is unavailable (OpenGL).` or the toolkit error text.

Options set before `mpv_initialize`: `vo=libmpv`, `idle=yes`, `pause=yes`, `mute=yes`, `keep-open=yes`, `config=no`, `load-scripts=no`, `ytdl=no`, `terminal=no`, `input-default-bindings=no`, `osc=no`, `hwdec=no`, `force-window=no`. The idle property read later is `idle-active`.

The render context is created on the toolkit thread after the area is realized, and only when that area has no GL error and EGL or GLX is current. A missing EGL context does not disable a GLX pane. `hwdec=auto-safe` is set only when EGL is current and the matching Wayland or X11 display pointer from GDK was passed. GLX stays on `hwdec=no` and does not receive a display pointer. Render does not set advanced control. Flip-Y is on after a non-zero framebuffer. The render signal returns true. If the player lock is busy, render schedules another draw instead of waiting.

One client thread serves every pane. The toolkit thread does not wait on it. Stop seeks to 0 and pauses. It does not send the `stop` command. At most one pane is audible. Leaving a page pauses and mutes the hidden group. Play and Stop do not start or kill FFmpeg.

### 2.6 Encode

Both Run buttons pause every pane and do not resume them. The visible result becomes `please wait for time consuming process` when Run starts. The window prints nothing for those lines. The GUI sink stores them. It does not turn on JSON mode.

Edit success is `SUCCESS → {full path}`. Edit failure is `Edit failed.` Child stderr stays off that label. Join shows the stored lines, including missing-file, same-video, output-is-source, and `ffmpeg:` lines. When those lines are empty, join success is `SUCCESS` and join failure is `Join failed.`

The cut bound is ffprobe duration, with end allowed through duration plus 0.001. The GUI percent sentence is `Percent must be 20–200.` The CLI sentence stays `ERROR: length percent must be from 20 to 200.` A video-only input omits the audio filter, the audio map, and `-c:a`. When ffprobe cannot say whether audio is present, the audio filter stays.

Close while a job is running waits. The delete handler returns true and sets the visible result to the same wait sentence. When the job finishes, a requested close destroys the window so each pane unrealizes before `gtk_main` returns. The encode thread does not touch toolkit widgets.

### 2.7 System log

The page shows the log folder from `ciao_log_directory()` and does not create that folder. View loads one selected file into a read-only text view. An empty selection says `Select a log file.` A path outside the folder says `That file is outside the log folder.` The folder prefix uses the separator from the real path: `\` on Windows and `/` elsewhere. A trailing `/` or `\` is already a prefix. Clear asks `Clear {name}?` with Yes and No, then deletes that one regular log file inside the folder. It does not clear every log. A failed delete says `Could not clear that log file.`

### 2.8 Implementation notes (this project)

| Item | Value |
|------|--------|
| Sources | `gui_dispatch.cpp`, `gui_format.cpp`, `gui_player.cpp`, `gui_window.cpp`, `video_job.h` |
| Open call | `gui_window_open` from `video_domain_run` when the route is the window |
| Binary | `target/exe/debug/VideoTools` |
| Panes | Five: edit source, edit generated, join video 1, join video 2, join generated |
| Clock | `gui_format_elapsed` prints `MM:SS.CC`. FFmpeg times stay `HH:MM:SS.mmm` |
| Chooser | `GtkFileChooserDialog` only |
| Verified display | A Linux session with GTK 3 drew one file inside the edit pane. Headless GUI end-to-end is not claimed |

### 2.9 Why this requirement exists

- **CIAO Principle 5** (https://github.com/cloudgen/ciao): one owner for the picture window, separate from the WebKit shell.
- **CIAO Principle 1**: a missing display or a missing libmpv fails closed with one sentence, and does not open the wrong window.

## Under command line for normal user only

When the product runs on Windows cmd, Git Bash, Termux, or the same class, this window does not gain admin privilege or a dedicated system user. It does not call `sudo`, `apt`, `winget`, or a package manager. On Windows cmd the same picture window is built only when that build has GTK, libmpv, and a display. A missing library leaves the panes on the unavailable label. Encode still uses `ffmpeg` from `PATH`.

**This requirement:** the picture window adds no install, no elevation, and no package-manager buttons.

## 3. Design principles

- **Caution**: `--window` and a mixed `--gemini` exit before GTK.
- **Intentional**: playback is libmpv inside the panes. Encode stays FFmpeg.
- **Anti-fragile**: a missing libmpv disables pictures and leaves join and edit able to encode.
- **Over-protect**: the toolkit thread never waits on the client thread, and the link line never needs libmpv.

## 4. Protection rule

Future editors must not move the WebKit or GStreamer block into this window, call `ciao_output_init` on the video path, spawn the `mpv` binary, pass `-lmpv`, set `GDK_BACKEND` inside the program, or treat `--window` as this window or as the inherited shell.

## 5. Related artifacts

| Artifact | Role |
|----------|------|
| `docs/requirements/index.md` | Registry |
| `docs/requirements/requirement-cpp-window.md` | Inherited WebKit shell |
| `docs/requirements/requirement-domain-videotools.md` | Verbs, menu, and encode jobs |
| `docs/requirements/requirement-cpp-windows.md` | Windows process and path contract |
| `docs/requirements/requirement-cpp-dependencies.md` | Link set. libmpv is headers and a runtime load, not a link |
| `src/main/cpp/gui_window.cpp` | Window |
| `src/main/cpp/gui_player.cpp` | Playback |
| `src/main/cpp/gui_dispatch.cpp` | Route |

**Last updated**: 2026-10-11
**Owner**: VideoTools picture window
