# Requirements registry

Product law for VideoTools, specialized from the gemini-wilgat bootstrap. Rows match files in this folder.

| Key | Area | Status | Path |
|-----|------|--------|------|
| requirement-class-software-dev | class | Active | requirement-class-software-dev.md |
| requirement-domain-videotools | domain | Active | requirement-domain-videotools.md |
| requirement-cpp-coding-style | coding | Active | requirement-cpp-coding-style.md |
| requirement-cpp-window | window | Active | requirement-cpp-window.md |
| requirement-cpp-gui | window | Active | requirement-cpp-gui.md |
| requirement-cpp-windows | windows | Active | requirement-cpp-windows.md |
| requirement-cpp-ui-menu | menu | Active | requirement-cpp-ui-menu.md |
| requirement-cpp-webui-auto-follow | webui | Active | requirement-cpp-webui-auto-follow.md |
| requirement-cpp-webui-bookmark | webui | Active | requirement-cpp-webui-bookmark.md |
| requirement-cpp-configuration | config | Active | requirement-cpp-configuration.md |
| requirement-cpp-output | output | Active | requirement-cpp-output.md |
| requirement-cpp-logger | logger | Deferred | requirement-cpp-logger.md |
| requirement-cpp-logger-folder | logger | Deferred | requirement-cpp-logger-folder.md |
| requirement-cpp-log-commands | logger | Active | requirement-cpp-log-commands.md |
| requirement-cpp-log-format | logger | Active | requirement-cpp-log-format.md |
| requirement-cpp-debug-mode | debug | Active | requirement-cpp-debug-mode.md |
| requirement-cpp-lua-script | lua | Active | requirement-cpp-lua-script.md |
| requirement-cpp-lua-script-commands | lua | Active | requirement-cpp-lua-script-commands.md |
| requirement-cpp-parameters | parameters | Active | requirement-cpp-parameters.md |
| requirement-cpp-database | persistence | Active | requirement-cpp-database.md |
| requirement-cpp-cookie-management | persistence | Active | requirement-cpp-cookie-management.md |
| requirement-cpp-project-structure | structure | Active | requirement-cpp-project-structure.md |
| requirement-cpp-milestones | milestones | Active | requirement-cpp-milestones.md |
| requirement-cpp-tests | tests | Active | requirement-cpp-tests.md |
| requirement-cpp-dependencies | dependencies | Active | requirement-cpp-dependencies.md |
| requirement-cpp-skills-installation-basic | build | Active | requirement-cpp-skills-installation-basic.md |
| requirement-cpp-skills-installation-meson | build | Active | requirement-cpp-skills-installation-meson.md |
| requirement-cpp-skills | skills | Deferred | requirement-cpp-skills.md |
| requirement-cpp-skills-installation-ant | build | Deferred | requirement-cpp-skills-installation-ant.md |
| requirement-cpp-skills-installation-gnu-make | build | Deferred | requirement-cpp-skills-installation-gnu-make.md |
| requirement-cpp-skills-installation-gradle | build | Deferred | requirement-cpp-skills-installation-gradle.md |

The `requirement-cpp-window` row is the single owner of the WebKit shell: start page, WebKit load order, which userscripts run, the GStreamer environment, and permission requests. `--gemini` opens that shell. Its runtime id is `VideoTools` (`PROJECT_ID` in `project.h`). Userscripts, cookies, and logs for that shell are under `~/.app/VideoTools/`. The `requirement-cpp-gui` row owns the video picture window. The `requirement-cpp-windows` row owns Windows process start and path joins. `--window` is an error.

The ui-menu row owns the menu bar: File (AutoClick, Bookmark, User Agent, Lua Script, Log, Exit) and About (Show Version). The About dialog stays unattached. Log's actions are the log-commands row. Lua Script's actions are the lua-script-commands row.

The webui-auto-follow row owns the Follow click script and the allow-list. That list is empty, so a page change does not click Follow.

The webui-bookmark row owns the bookmark list: one item, Start Page, and the `--bookmark` index.

The lua-script row owns the C++ host: one sol2 state in `userscript_manager.cpp`. The lua-script-commands row owns File → Lua Script and the `luascript` verb (`folder`, `list`, `view`, `edit`, `new`) in text and in `--json`. New asks for the filename in a dialog. That verb is in the help text. The menu and the verb are in the binary. `TP-LUA-SCRIPT-COMMANDS` is done.

The dependencies row owns the libraries the C/C++ program links: GTK 3, WebKitGTK 4.1 (libwebkit2gtk), GLib, SQLite (libsqlite3), Lua 5.3 (liblua), epoxy, threads, `-ldl`, and the sol2 headers. libmpv headers are a compile input. The link line does not pass `-lmpv`. The binary loads `libmpv.so.2` at runtime.

Deferred rows are not current behavior. `./build.sh` implements `install`, `clean`, and `build` only.

The logger row owns the stored line `[YYYY-MM-DD HH:MM:SS] pid:<pid> [LEVEL] @<file> :] message`, where `<file>` is the base name of the source file that logged it, the file `VideoTools-<YYYYMMDD>.log`, and the five project log content types: info, warning, error, debug, and gnome-debug. The gnome-debug examples in this project are `Gtk-CRITICAL` in `src/main/cpp/url_bar_user_agent.cpp` and `g_warning("[AUTOCLICK] Cannot find valid web_view")` in `src/main/cpp/menu_bookmark_app.cpp`. The logger-folder row owns `/var/VideoTools/log` for root or sudo and `$HOME/.app/VideoTools/log` for every other user. The terminal line stays with the output row. `test`, `list`, and `show` are not in the help text. The log-commands row owns the File → Log menu and the `log` verb (`list`, `show N`, `folder`, `clear`) in text and in `--json`. That verb is in the help text. It does not implement `test`, `list`, or `show`. `log show N` prints the file bytes with no rewrite. File → Log → List log files loads the HTML log table into the main web view: header Time, Pid, Level, Source, Message; odd rows `#ffffff`; even rows `#e7eef6`; hover `#ffe08a`; displayed cells omit `[` `]` `<` `>` `(` `)` `{` `}`. File → Log → Log folder opens that directory in the desktop file manager. The `log folder` command still prints the path. The log-format row owns the checker for that stored line and the test-purpose verb `logcheck`. Help lists `logcheck` under Tests, apart from `log`. File → Log still loads the table without that checker. The address bar Go button runs the checker when the request is a local file whose name ends in `.log`. A pass loads the same table. A fail does not.
