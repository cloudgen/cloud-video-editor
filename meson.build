# meson.build — second build for cloud-video-editor.
# Compiles the same C++ sources as the g++ line in ./build.sh.
# project() version matches the runtime macros in src/main/cpp/project.h (1.0.0).

project('cloud-video-editor', 'cpp',
    version: '1.0.0',
    license: 'MIT',
    default_options: [
        'warning_level=3',
        'buildtype=debugoptimized',
        'cpp_std=c++17',
    ]
)

gtk_dep = dependency('gtk+-3.0', required: true)
webkit_dep = dependency('webkit2gtk-4.1', required: true)
glib_dep = dependency('glib-2.0', required: true)
gobject_dep = dependency('gobject-2.0', required: true)
gio_dep = dependency('gio-2.0', required: true)
sqlite_dep = dependency('sqlite3', required: true)
lua_dep = dependency('lua5.3', required: true)
thread_dep = dependency('threads', required: true)
epoxy_dep = dependency('epoxy', required: true)
# Headers only. The binary loads libmpv.so.2 with dlopen and must not gain NEEDED libmpv.
# pkg-config --exists often fails when Requires.private packages are absent.
# Use --cflags, then MPV_INCLUDE_DIR, /usr/include, and /usr/local/include.
# Never dependency('mpv'): that puts libmpv on the link line.
mpv_compile_args = []
mpv_cflags_cmd = run_command('pkg-config', '--cflags', 'mpv', check: false)
if mpv_cflags_cmd.returncode() == 0 and mpv_cflags_cmd.stdout().strip() != ''
    mpv_compile_args = mpv_cflags_cmd.stdout().split()
else
    cpp_compiler = meson.get_compiler('cpp')
    mpv_include_candidates = []
    mpv_include_dir = run_command('printenv', 'MPV_INCLUDE_DIR', check: false).stdout().strip()
    if mpv_include_dir != ''
        mpv_include_candidates += [mpv_include_dir]
    endif
    mpv_include_candidates += ['/usr/include', '/usr/local/include']
    foreach dir : mpv_include_candidates
        if mpv_compile_args.length() == 0 and cpp_compiler.has_header('mpv/client.h', args: ['-I' + dir])
            mpv_compile_args = ['-I' + dir]
        endif
    endforeach
endif
if mpv_compile_args.length() == 0
    error('mpv/client.h was not found. Install libmpv-dev or set MPV_INCLUDE_DIR. Do not link -lmpv.')
endif
mpv_headers = declare_dependency(compile_args: mpv_compile_args)
dl_dep = meson.get_compiler('cpp').find_library('dl', required: true)

sol_inc = include_directories('/usr/local/include')

src_files = files(
    'src/main/cpp/auto_follow.cpp',
    'src/main/cpp/config.cpp',
    'src/main/cpp/cookies.cpp',
    'src/main/cpp/dialog_about.cpp',
    'src/main/cpp/dialog_version.cpp',
    'src/main/cpp/load_page_ogin_check.cpp',
    'src/main/cpp/login_detector_app.cpp',
    'src/main/cpp/log_commands.cpp',
    'src/main/cpp/log_format.cpp',
    'src/main/cpp/log_view_html.cpp',
    'src/main/cpp/lua_script_commands.cpp',
    'src/main/cpp/main.cpp',
    'src/main/cpp/menu_bookmark.cpp',
    'src/main/cpp/menu_bookmark_app.cpp',
    'src/main/cpp/menu_exit.cpp',
    'src/main/cpp/menu_log.cpp',
    'src/main/cpp/menu_lua_script.cpp',
    'src/main/cpp/menu_user_agent.cpp',
    'src/main/cpp/output.cpp',
    'src/main/cpp/path_utils.cpp',
    'src/main/cpp/settings.cpp',
    'src/main/cpp/ui_menu_app.cpp',
    'src/main/cpp/ui_window_app.cpp',
    'src/main/cpp/url_bar_go_button.cpp',
    'src/main/cpp/url_bar_page.cpp',
    'src/main/cpp/url_bar_user_agent.cpp',
    'src/main/cpp/webkit_setup.cpp',
    'src/main/cpp/userscript_manager.cpp',
    'src/main/cpp/video_domain.cpp',
    'src/main/cpp/video_host.cpp',
    'src/main/cpp/gui_dispatch.cpp',
    'src/main/cpp/gui_format.cpp',
    'src/main/cpp/gui_player.cpp',
    'src/main/cpp/gui_window.cpp',
)

executable('cloud-video-editor',
    sources: src_files,
    include_directories: sol_inc,
    dependencies: [
        gtk_dep,
        webkit_dep,
        glib_dep,
        gobject_dep,
        gio_dep,
        sqlite_dep,
        lua_dep,
        thread_dep,
        epoxy_dep,
        mpv_headers,
        dl_dep,
    ],
    install: true,
    install_dir: get_option('bindir')
)
