**file**: docs/requirements/requirement-domain-videotools.md
**Status**: Active (Version 1.0.0)
**Area**: domain
**Key**: `requirement-domain-videotools`
**Philosophy**: CIAO v2.10.2 / CIAO-Lite (Caution • Intentional • Anti-fragile • Over-engineered / Over-protect)

## 1. Purpose

This file is the domain surface for the C++ cloud-video-editor program built in this checkout. The architecture comes from the gemini-wilgat bootstrap. The jobs are the join and edit steps. `src/main/cpp/video_domain.cpp` is the ship unit for those jobs. A bare start on a terminal with a display opens the picture window in `requirement-cpp-gui.md`. The inherited wilgat window stays behind `--gemini`, `log`, `luascript`, and `logcheck`. `--window` is an error.

### 1.1 Human-facing

**In one sentence:** cloud-video-editor joins two videos in the current folder, and it can cut, speed, or boomerang one MP4.

| Includes | Excludes |
|----------|----------|
| Two-file join in the current folder. Formats `.mp4` `.mov` `.mkv` `.avi` `.m4v` | Joining more than two files, a recursive join scan, cut or speed inside join |
| Edit of one MP4: cut, length percent 20–200, one tail (none, boomerang, or repeat) | Non-MP4 as an edit input, both tails at once |
| `list-videos` and `list-mp4` | Encoding from either list command |

| You do… | What it means | What you type |
|---------|---------------|---------------|
| Join two videos | Name both files, or answer the questions on a terminal | `cloud-video-editor join --first a.mp4 --second b.mp4` |
| See what can be joined | List eligible files in this folder and stop | `cloud-video-editor list-videos` |
| Cut and speed one file | Pass the file, start, end, and optional percent and tail | `cloud-video-editor edit --file clip.mp4 --start 1 --end 5 --percent 50` |
| See MP4 files | List MP4 files under a folder and stop | `cloud-video-editor list-mp4` |
| Read about | Name, version, both jobs, FFmpeg, ffprobe | `cloud-video-editor about` |

## 2. Core rules

### 2.1 Specialized CLI subcommands

| Verb | Behavior |
|------|----------|
| *(none)* | Both terminal sides and a display (`WAYLAND_DISPLAY` or `DISPLAY`): picture window. No display, or standard output is not a terminal: numbered menu. Standard input is not a terminal: help and return 0. A complete `--file` `--start` `--end` set encodes and does not show the menu |
| `join` | Two videos in the current folder. Stream copy first, then re-encode. With no terminal and no `--first` / `--second`, exit 1 |
| `edit` | One MP4. Cut, then speed, then one tail |
| `list-videos` | Print join-eligible names. Do not encode |
| `list-mp4` | Print MP4 paths under `--folder` or the current folder. Do not encode |
| `help` | Print the catalog below and return 0 |
| `about` | Print the about lines and return 0 |
| `version` | Print `cloud-video-editor` and `1.0.0` |
| `tui` | The numbered menu. With no terminal, exit 1 |

Flags: `--file`, `--folder`, `--start`, `--end`, `--percent`, `--boomerang`, `--repeat`, `--first`, `--second`, `--output`, `--json`.

`--percent`, `--boomerang`, or `--repeat` without `--file`, `--start`, and `--end` exits 1. `--boomerang` and `--repeat` together exit 1.

`--json` prints one object and no other stdout. The object has `ok`, `mode`, `app`, `version`, `error`, `next`, `files`, and `jobs`.

`log`, `luascript`, and `logcheck` are not domain verbs. They keep the bootstrap shell. `--gemini` alone opens that shell. `--gemini` with another mode exits 1. `--window` exits 1 and opens nothing. `tui` shows the menu when standard input is a terminal, and exits 1 otherwise.

### 2.2 Specialized features

| Feature | Rule |
|---------|------|
| Join discovery | Current folder only. Case-insensitive suffixes `.mp4` `.mov` `.mkv` `.avi` `.m4v`. Not recursive |
| Two-file selection | The second pick cannot be the same path as the first |
| Join output | Empty name becomes `{stem1} + {stem2}.mp4`. A name that does not end in `.mp4`, `.mkv`, or `.mov` gains `.mp4` |
| Join encode | `ffmpeg` stream copy, then `libx264` / AAC re-encode if copy fails. Fail closed if both fail |
| Edit discovery | Recursive `*.mp4` under the chosen folder |
| Duration | `ffprobe` format duration. Fail closed when the probe is missing or the length is not positive |
| Cut | `0 <= start < end <= duration` |
| Length percent | Default 100. Allowed 20–200. 100 keeps the cut and skips the speed encode |
| Boomerang | Append the reverse of the forward clip |
| Repeat | Append the forward clip once |
| Edit output | `{stem}_cut{start:.1f}-{end:.1f}s_{int(percent)}pct[_BOOMERANG\|_REPEAT].mp4` beside the source |
| Wait line | `please wait for time consuming process` while an FFmpeg child runs |
| FFmpeg | Fail closed when a join or an edit encodes and `ffmpeg` is not on `PATH` |
| Temps | Unique files beside the output when that folder is writable, otherwise `$TMPDIR` or `/tmp`. Publish with `rename`, and copy plus delete when the mount differs |

### 2.3 Specialized help items

`cloud-video-editor help` must state: join is the current folder only; the five join suffixes; first video then second; the default join name; `list-videos` does not join; stream copy then re-encode; edit cut, percent 20–200, one tail; MP4 only; `list-mp4` does not encode; the edit filename; the flags; `--json`; FFmpeg and ffprobe; a bare start with a display opens the picture window; with no display, or when standard output is not a terminal, that start shows the menu; `tui` always shows the menu; join does not cut or speed; `log`, `luascript`, and `logcheck` keep the shell; `--gemini` opens that shell; `--window` is an error.

### 2.4 Specialized about items

| Field | Content |
|-------|---------|
| Product name | cloud-video-editor |
| Version | `1.0.0` from `project.h` |
| Domain summary | Join two local videos, and cut, speed, or boomerang an MP4 |
| Runtime tools | FFmpeg for copy, re-encode, cut, and speed. ffprobe for duration |
| Entry | `cloud-video-editor` |
| Bootstrap | A bare start on a terminal with a display opens the picture window. `--gemini` opens the gemini-wilgat shell. About says that. `--window` is an error |

### 2.5 Implementation notes (this project)

| Item | Value |
|------|--------|
| Checkout | This workspace (`cloud-video-editor`). The Python project at the sibling `VideoTools` root is a different ship unit and is not overwritten |
| Bootstrap origin | `gemini-wilgat`, left unchanged |
| Domain source | `src/main/cpp/video_domain.cpp` |
| Binary | `target/exe/debug/cloud-video-editor`, produced by `g++ -std=c++17` |
| Version | `1.0.0` |
| Menu | Numbered rows 1 join, 2 edit, 3 list-videos, 4 list-mp4, 5 help, 6 about, 7 version, 9 Exit. Not the thirteen-language painter |
| Duration tool | `ffprobe`, not an OpenCV link |

## 3. Protection rule

Do not point this product's default start at the Gemini page. Do not copy this domain back onto the gemini-wilgat bootstrap.

## 4. Related artifacts

| Artifact | Role |
|----------|------|
| `docs/requirements/index.md` | Registry |
| `src/main/cpp/project.h` | `PROJECT_ID` and version |
| `docs/requirements/requirement-cpp-gui.md` | Picture window |
| `docs/requirements/requirement-cpp-windows.md` | Windows process and path contract |
| `src/main/cpp/main.cpp` | Sends domain starts here. `--gemini` reaches the wilgat shell. `--window` does not |

**Last updated**: 2026-10-11
**Owner**: cloud-video-editor domain
