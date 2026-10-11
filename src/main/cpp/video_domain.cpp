// video_domain.cpp
// VideoTools domain: join two videos in this folder, and cut, speed, or
// boomerang one MP4. Compiled by g++ with the gemini-wilgat bootstrap shell.
// FFmpeg does the encode. Duration comes from ffprobe.

#include "video_domain.h"
#include "project.h"
#include "gui_dispatch.h"
#include "gui_window.h"
#include "video_host.h"
#include "video_job.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

#if !defined(S_ISREG)
#define S_ISREG(m) (((m) & _S_IFMT) == _S_IFREG)
#endif
#if !defined(S_ISDIR)
#define S_ISDIR(m) (((m) & _S_IFMT) == _S_IFDIR)
#endif
#if !defined(S_ISLNK)
#define S_ISLNK(m) (((m) & _S_IFMT) == _S_IFLNK)
#endif

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <direct.h>
#include <io.h>
#else
#include <dirent.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

const char *WAIT_PHRASE = "please wait for time consuming process";
const double PERCENT_MIN = 20.0;
const double PERCENT_MAX = 200.0;

struct Job {
    std::string file;
    double start;
    double end;
    double percent;
    std::string tail;
    std::string output;
};

struct State {
    bool json = false;
    bool gui = false;
    std::string mode;
    std::string error;
    std::string next;
    std::vector<Job> jobs;
    std::vector<std::string> files;
    std::vector<std::string> gui_lines;
};

std::string one_line(const std::string &message)
{
    std::string line = message;
    while (!line.empty() && (line[line.size() - 1] == '\n' || line[line.size() - 1] == '\r')) {
        line.erase(line.size() - 1);
    }
    return line;
}

std::string version_text()
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    return buf;
}

std::string json_escape(const std::string &text)
{
    std::string out;
    out.reserve(text.size() + 8);
    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char hex[8];
                std::snprintf(hex, sizeof(hex), "\\u%04x", c);
                out += hex;
            } else {
                out += static_cast<char>(c);
            }
        }
    }
    return out;
}

void remember(State &state, const std::string &message, const std::string &next)
{
    if (!state.json) {
        return;
    }
    if (state.error.empty() && !message.empty()) {
        state.error = message;
    }
    if (state.next.empty() && !next.empty()) {
        state.next = next;
    }
}

void out_info(State &state, const std::string &message)
{
    if (state.gui) {
        state.gui_lines.push_back(one_line(message));
        return;
    }
    if (state.json) {
        return;
    }
    std::fputs(message.c_str(), stdout);
    if (message.empty() || message[message.size() - 1] != '\n') {
        std::fputc('\n', stdout);
    }
}

void out_err(State &state, const std::string &message)
{
    if (state.gui) {
        std::string line = one_line(message);
        state.gui_lines.push_back(line);
        if (state.error.empty() && !line.empty()) {
            state.error = line;
        }
        return;
    }
    if (state.json) {
        remember(state, message, "");
        return;
    }
    std::fputs(message.c_str(), stderr);
    if (message.empty() || message[message.size() - 1] != '\n') {
        std::fputc('\n', stderr);
    }
}

int emit_json(State &state, int code)
{
    bool ok = (code == 0 && state.error.empty());
    std::ostringstream doc;
    doc << "{\n";
    doc << "  \"ok\": " << (ok ? "true" : "false") << ",\n";
    doc << "  \"mode\": \"" << json_escape(state.mode) << "\",\n";
    doc << "  \"app\": \"" << PROJECT_NAME << "\",\n";
    doc << "  \"version\": \"" << version_text() << "\",\n";
    doc << "  \"error\": ";
    if (state.error.empty()) {
        doc << "null";
    } else {
        doc << "\"" << json_escape(state.error) << "\"";
    }
    doc << ",\n  \"next\": ";
    if (state.next.empty()) {
        doc << "null";
    } else {
        doc << "\"" << json_escape(state.next) << "\"";
    }
    doc << ",\n  \"files\": [";
    for (size_t i = 0; i < state.files.size(); ++i) {
        if (i) {
            doc << ", ";
        }
        doc << "\"" << json_escape(state.files[i]) << "\"";
    }
    doc << "],\n  \"jobs\": [";
    for (size_t i = 0; i < state.jobs.size(); ++i) {
        const Job &job = state.jobs[i];
        if (i) {
            doc << ", ";
        }
        doc << "{\"file\":\"" << json_escape(job.file) << "\""
            << ",\"start\":" << job.start
            << ",\"end\":" << job.end
            << ",\"percent\":" << job.percent
            << ",\"boomerang\":" << (job.tail == "boomerang" ? "true" : "false")
            << ",\"tail\":\"" << json_escape(job.tail) << "\""
            << ",\"output\":\"" << json_escape(job.output) << "\"}";
    }
    doc << "]\n}\n";
    std::fputs(doc.str().c_str(), stdout);
    if (ok) {
        return 0;
    }
    return code == 0 ? 1 : code;
}

int finish(State &state, int code)
{
    if (state.json) {
        return emit_json(state, code);
    }
    return code;
}

bool on_path(const char *name)
{
    return host_executable_on_path(name);
}

bool is_tty_in()
{
#if defined(_WIN32)
    return _isatty(_fileno(stdin)) != 0;
#else
    return isatty(STDIN_FILENO) == 1;
#endif
}

bool is_tty_out()
{
#if defined(_WIN32)
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(STDOUT_FILENO) == 1;
#endif
}

bool read_line(const char *prompt, std::string &out)
{
    if (!is_tty_in()) {
        return false;
    }
    std::fputs(prompt, stdout);
    std::fflush(stdout);
    char buf[4096];
    if (std::fgets(buf, sizeof(buf), stdin) == NULL) {
        return false;
    }
    out = buf;
    while (!out.empty() && (out[out.size() - 1] == '\n' || out[out.size() - 1] == '\r')) {
        out.erase(out.size() - 1);
    }
    return true;
}

std::string lower_copy(std::string text)
{
    for (size_t i = 0; i < text.size(); ++i) {
        text[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
    }
    return text;
}

std::string base_name(const std::string &path)
{
    return host_basename(path);
}

std::string stem_of(const std::string &path)
{
    std::string base = base_name(path);
    size_t dot = base.find_last_of('.');
    if (dot == std::string::npos || dot == 0) {
        return base;
    }
    return base.substr(0, dot);
}

std::string suffix_lower(const std::string &path)
{
    std::string base = base_name(path);
    size_t dot = base.find_last_of('.');
    if (dot == std::string::npos) {
        return "";
    }
    return lower_copy(base.substr(dot));
}

bool suffix_is(const std::string &path, const char *const *allowed)
{
    std::string suffix = suffix_lower(path);
    for (size_t i = 0; allowed[i] != NULL; ++i) {
        if (suffix == allowed[i]) {
            return true;
        }
    }
    return false;
}

bool is_regular_file(const std::string &path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return false;
    }
    return S_ISREG(st.st_mode);
}

bool is_dir(const std::string &path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return false;
    }
    return S_ISDIR(st.st_mode);
}

long file_size(const std::string &path)
{
    struct stat st;
    if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
        return -1;
    }
    return static_cast<long>(st.st_size);
}

std::string real_path(const std::string &path)
{
    return host_real_path(path);
}

std::string parent_dir(const std::string &path)
{
    return host_parent_dir(path);
}

std::string join_path(const std::string &dir, const std::string &name)
{
    return host_join_path(dir, name);
}

bool ensure_ffmpeg(State &state)
{
    if (!on_path("ffmpeg")) {
        out_err(state, "ERROR: ffmpeg not found on PATH.");
        out_err(state, "   Install FFmpeg and ensure `ffmpeg` is available.");
        out_err(state, "   → https://ffmpeg.org/download.html");
        remember(state, "ffmpeg not found on PATH.", "Install FFmpeg and ensure ffmpeg is available.");
        return false;
    }
    return true;
}

int run_process(const std::vector<std::string> &args, std::string &err_text)
{
    std::string captured;
    int code = host_spawn(args, captured, HOST_CAPTURE_STDERR);
    if (code == 127 && (captured == "empty command" || captured == "pipe failed" ||
                        captured == "fork failed" || captured == "wait failed" ||
                        captured == "CreateProcessW failed")) {
        err_text = captured;
        return code;
    }
    if (captured == "ffmpeg did not exit") {
        err_text = captured;
        return code;
    }
    if (!captured.empty()) {
        size_t start = captured.size() > 500 ? captured.size() - 500 : 0;
        std::string clip = captured.substr(start);
        size_t line = clip.find_last_of('\n');
        if (line != std::string::npos && line + 1 < clip.size()) {
            err_text = clip.substr(line + 1);
        } else {
            err_text = clip;
        }
        while (!err_text.empty() && (err_text[err_text.size() - 1] == '\n' || err_text[err_text.size() - 1] == '\r')) {
            err_text.erase(err_text.size() - 1);
        }
    }
    return code;
}

bool run_ffmpeg(State &state, const std::vector<std::string> &cmd, std::string &err_text)
{
    if (!state.json && !state.gui) {
        std::fputs("   ● ", stdout);
        std::fputs(WAIT_PHRASE, stdout);
        std::fputc('\n', stdout);
        std::fflush(stdout);
    }
    int code = run_process(cmd, err_text);
    return code == 0;
}

std::string make_temp(const std::string &near, const char *suffix, std::string &err)
{
    std::string folder = parent_dir(near);
#if defined(_WIN32)
    if (!is_dir(folder) || _access(folder.c_str(), 2) != 0) {
#else
    if (!is_dir(folder) || access(folder.c_str(), W_OK) != 0) {
#endif
        folder = host_temp_dir();
    }
    return host_make_temp(folder, suffix, err);
}

void discard_file(const std::string &path)
{
    if (!path.empty()) {
#if defined(_WIN32)
        _unlink(path.c_str());
#else
        unlink(path.c_str());
#endif
    }
}

bool promote_file(const std::string &src, const std::string &dest, std::string &err)
{
    if (file_size(src) <= 0) {
        err = "promote source missing";
        return false;
    }
    if (rename(src.c_str(), dest.c_str()) == 0) {
        return true;
    }
    if (errno != EXDEV) {
        err = std::strerror(errno);
        return false;
    }
    FILE *in = std::fopen(src.c_str(), "rb");
    FILE *out = std::fopen(dest.c_str(), "wb");
    if (in == NULL || out == NULL) {
        if (in) {
            std::fclose(in);
        }
        if (out) {
            std::fclose(out);
        }
        err = "cross-filesystem publish failed";
        return false;
    }
    char buf[8192];
    size_t nread = 0;
    bool ok = true;
    while ((nread = std::fread(buf, 1, sizeof(buf), in)) > 0) {
        if (std::fwrite(buf, 1, nread, out) != nread) {
            ok = false;
            break;
        }
    }
    if (std::ferror(in)) {
        ok = false;
    }
    std::fclose(in);
    if (std::fclose(out) != 0) {
        ok = false;
    }
    if (!ok) {
#if defined(_WIN32)
        _unlink(dest.c_str());
#else
        unlink(dest.c_str());
#endif
        err = "cross-filesystem publish failed";
        return false;
    }
#if defined(_WIN32)
    _unlink(src.c_str());
#else
    unlink(src.c_str());
#endif
    return true;
}

std::string concat_quote(const std::string &path)
{
    std::string out;
    for (size_t i = 0; i < path.size(); ++i) {
        if (path[i] == '\'') {
            out += "'\\''";
        } else {
            out += path[i];
        }
    }
    return out;
}

bool write_concat_list(const std::string &list_path, const std::vector<std::string> &parts, std::string &err)
{
    FILE *handle = std::fopen(list_path.c_str(), "w");
    if (handle == NULL) {
        err = "could not write the concat list";
        return false;
    }
    for (size_t i = 0; i < parts.size(); ++i) {
        std::string abs = real_path(parts[i]);
        if (abs.empty()) {
            std::fclose(handle);
            err = "could not resolve a concat part";
            return false;
        }
        std::fprintf(handle, "file '%s'\n", concat_quote(abs).c_str());
    }
    std::fclose(handle);
    return true;
}

const char *JOIN_SUFFIXES[] = {".mp4", ".mov", ".mkv", ".avi", ".m4v", NULL};
const char *OUT_SUFFIXES[] = {".mp4", ".mkv", ".mov", NULL};

void discover_dir(const std::string &directory, const char *const *suffixes, bool recursive, std::vector<std::string> &found)
{
#if defined(_WIN32)
    int chars = MultiByteToWideChar(CP_UTF8, 0, directory.c_str(), -1, NULL, 0);
    if (chars <= 0) {
        return;
    }
    std::wstring folder(static_cast<size_t>(chars), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, directory.c_str(), -1, &folder[0], chars);
    if (!folder.empty() && folder[folder.size() - 1] == L'\0') {
        folder.erase(folder.size() - 1);
    }
    std::wstring pattern = folder;
    if (!pattern.empty() && pattern[pattern.size() - 1] != L'\\' && pattern[pattern.size() - 1] != L'/') {
        pattern.push_back(L'\\');
    }
    pattern += L"*";
    WIN32_FIND_DATAW data;
    HANDLE handle = FindFirstFileW(pattern.c_str(), &data);
    if (handle == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        if (std::wcscmp(data.cFileName, L".") == 0 || std::wcscmp(data.cFileName, L"..") == 0) {
            continue;
        }
        int bytes = WideCharToMultiByte(CP_UTF8, 0, data.cFileName, -1, NULL, 0, NULL, NULL);
        if (bytes <= 0) {
            continue;
        }
        std::string name(static_cast<size_t>(bytes), '\0');
        WideCharToMultiByte(CP_UTF8, 0, data.cFileName, -1, &name[0], bytes, NULL, NULL);
        if (!name.empty() && name[name.size() - 1] == '\0') {
            name.erase(name.size() - 1);
        }
        std::string path = join_path(directory, name);
        bool directory_entry = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        bool link = (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
        if (!directory_entry && suffix_is(path, suffixes)) {
            found.push_back(path);
        } else if (recursive && directory_entry && !link) {
            discover_dir(path, suffixes, true, found);
        }
    } while (FindNextFileW(handle, &data));
    FindClose(handle);
#else
    DIR *dir = opendir(directory.c_str());
    if (dir == NULL) {
        return;
    }
    struct dirent *entry = NULL;
    while ((entry = readdir(dir)) != NULL) {
        if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        std::string path = join_path(directory, entry->d_name);
        struct stat st;
        if (lstat(path.c_str(), &st) != 0) {
            continue;
        }
        if (S_ISREG(st.st_mode) && suffix_is(path, suffixes)) {
            found.push_back(path);
        } else if (recursive && S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode)) {
            discover_dir(path, suffixes, true, found);
        }
    }
    closedir(dir);
#endif
}

bool name_less(const std::string &a, const std::string &b)
{
    return lower_copy(base_name(a)) < lower_copy(base_name(b));
}

std::vector<std::string> list_join_videos(const std::string &directory)
{
    std::vector<std::string> found;
    discover_dir(directory.empty() ? "." : directory, JOIN_SUFFIXES, false, found);
    std::sort(found.begin(), found.end(), name_less);
    return found;
}

const char *MP4_SUFFIXES[] = {".mp4", NULL};

std::vector<std::string> list_mp4(const std::string &directory)
{
    std::vector<std::string> found;
    discover_dir(directory.empty() ? "." : directory, MP4_SUFFIXES, true, found);
    std::sort(found.begin(), found.end(), name_less);
    std::vector<std::string> unique;
    std::vector<std::string> seen;
    for (size_t i = 0; i < found.size(); ++i) {
        std::string key = real_path(found[i]);
        if (key.empty()) {
            key = found[i];
        }
        bool dup = false;
        for (size_t j = 0; j < seen.size(); ++j) {
            if (seen[j] == key) {
                dup = true;
                break;
            }
        }
        if (!dup) {
            seen.push_back(key);
            unique.push_back(found[i]);
        }
    }
    return unique;
}

std::string resolve_output_name(const std::string &vid1, const std::string &vid2, const std::string &raw)
{
    std::string name = raw;
    if (name.empty()) {
        name = stem_of(vid1) + " + " + stem_of(vid2) + ".mp4";
    }
    if (!suffix_is(name, OUT_SUFFIXES)) {
        name += ".mp4";
    }
    return name;
}

bool same_file(const std::string &a, const std::string &b)
{
    std::string ra = real_path(a);
    std::string rb = real_path(b);
    if (!ra.empty() && !rb.empty()) {
        return ra == rb;
    }
    return a == b;
}

enum AudioProbe { AUDIO_YES = 0, AUDIO_NO, AUDIO_UNKNOWN };
AudioProbe probe_audio(const std::string &path);
double probe_duration(State &state, const std::string &path);

bool join_files(State &state, const std::string &vid1, const std::string &vid2, const std::string &out_path)
{
    state.mode = "join";
    if (!is_regular_file(vid1) || !is_regular_file(vid2)) {
        out_err(state, "ERROR: one or both input videos are missing.");
        remember(state, "one or both input videos are missing.", "");
        return false;
    }
    if (same_file(vid1, vid2)) {
        out_err(state, "ERROR: the same video cannot be chosen twice.");
        remember(state, "the same video cannot be chosen twice.", "");
        return false;
    }
    if (is_regular_file(out_path) && (same_file(out_path, vid1) || same_file(out_path, vid2))) {
        out_err(state, "ERROR: output path must not be one of the source files.");
        remember(state, "output path must not be one of the source files.", "");
        return false;
    }
    if (!ensure_ffmpeg(state)) {
        return false;
    }
    std::string err;
    std::string list_path = make_temp(out_path, ".txt", err);
    std::string copy_temp = make_temp(out_path, ".mp4", err);
    if (list_path.empty() || copy_temp.empty()) {
        discard_file(list_path);
        discard_file(copy_temp);
        out_err(state, err.empty() ? "could not create a temp file" : err);
        return false;
    }
    std::vector<std::string> parts;
    parts.push_back(vid1);
    parts.push_back(vid2);
    if (!write_concat_list(list_path, parts, err)) {
        discard_file(list_path);
        discard_file(copy_temp);
        out_err(state, err);
        return false;
    }
    out_info(state, "Running ffmpeg (stream copy – no quality loss)…");
    std::vector<std::string> cmd_copy;
    cmd_copy.push_back("ffmpeg");
    cmd_copy.push_back("-nostdin");
    cmd_copy.push_back("-y");
    cmd_copy.push_back("-hide_banner");
    cmd_copy.push_back("-loglevel");
    cmd_copy.push_back("error");
    cmd_copy.push_back("-f");
    cmd_copy.push_back("concat");
    cmd_copy.push_back("-safe");
    cmd_copy.push_back("0");
    cmd_copy.push_back("-i");
    cmd_copy.push_back(list_path);
    cmd_copy.push_back("-c");
    cmd_copy.push_back("copy");
    cmd_copy.push_back("-map");
    cmd_copy.push_back("0:v");
    cmd_copy.push_back("-map");
    cmd_copy.push_back("0:a?");
    cmd_copy.push_back(copy_temp);
    std::string copy_err;
    bool copied = run_ffmpeg(state, cmd_copy, copy_err) && file_size(copy_temp) > 0;
    if (copied) {
        if (!promote_file(copy_temp, out_path, err)) {
            discard_file(list_path);
            discard_file(copy_temp);
            out_err(state, "Job failed: " + err);
            return false;
        }
        discard_file(list_path);
        out_info(state, "SUCCESS! Perfectly joined with original sound → " + out_path);
        Job job;
        job.file = vid1;
        job.start = 0;
        job.end = 0;
        job.percent = 100;
        job.tail = "none";
        job.output = out_path;
        state.jobs.push_back(job);
        return true;
    }
    discard_file(copy_temp);
    out_info(state, "Fast method failed (different resolutions/codec?). Trying safe re-encode...");
    if (!copy_err.empty()) {
        out_info(state, "   ffmpeg: " + copy_err);
    }
    std::string reenc_temp = make_temp(out_path, ".mp4", err);
    if (reenc_temp.empty()) {
        discard_file(list_path);
        out_err(state, err);
        return false;
    }
    std::vector<std::string> cmd_re;
    cmd_re.push_back("ffmpeg");
    cmd_re.push_back("-nostdin");
    cmd_re.push_back("-y");
    cmd_re.push_back("-hide_banner");
    cmd_re.push_back("-loglevel");
    cmd_re.push_back("error");
    AudioProbe audio0 = probe_audio(vid1);
    AudioProbe audio1 = probe_audio(vid2);
    bool both_silent = audio0 == AUDIO_NO && audio1 == AUDIO_NO;
    bool mixed = (audio0 == AUDIO_YES && audio1 == AUDIO_NO) || (audio0 == AUDIO_NO && audio1 == AUDIO_YES);
    bool map_audio = !both_silent;
    std::string filter = "[0:v][0:a][1:v][1:a]concat=n=2:v=1:a=1[v][a]";
    std::string silent_seconds;
    if (both_silent) {
        filter = "[0:v][1:v]concat=n=2:v=1:a=0[v]";
    } else if (mixed) {
        const std::string &silent = audio0 == AUDIO_NO ? vid1 : vid2;
        double silent_dur = probe_duration(state, silent);
        if (silent_dur <= 0.0) {
            discard_file(list_path);
            discard_file(reenc_temp);
            out_err(state, "ERROR: Could not read duration for " + base_name(silent));
            remember(state, "Could not read duration for " + base_name(silent), "");
            return false;
        }
        char tbuf[64];
        std::snprintf(tbuf, sizeof(tbuf), "%.6f", silent_dur);
        silent_seconds = tbuf;
        if (audio0 == AUDIO_YES) {
            filter = "[0:v][0:a][1:v][2:a]concat=n=2:v=1:a=1[v][a]";
        } else {
            filter = "[0:v][2:a][1:v][1:a]concat=n=2:v=1:a=1[v][a]";
        }
    }
    cmd_re.push_back("-i");
    cmd_re.push_back(vid1);
    cmd_re.push_back("-i");
    cmd_re.push_back(vid2);
    if (mixed) {
        cmd_re.push_back("-f");
        cmd_re.push_back("lavfi");
        cmd_re.push_back("-t");
        cmd_re.push_back(silent_seconds);
        cmd_re.push_back("-i");
        cmd_re.push_back("anullsrc=channel_layout=stereo:sample_rate=48000");
    }
    cmd_re.push_back("-filter_complex");
    cmd_re.push_back(filter);
    cmd_re.push_back("-c:v");
    cmd_re.push_back("libx264");
    cmd_re.push_back("-preset");
    cmd_re.push_back("fast");
    cmd_re.push_back("-crf");
    cmd_re.push_back("18");
    if (map_audio) {
        cmd_re.push_back("-c:a");
        cmd_re.push_back("aac");
        cmd_re.push_back("-b:a");
        cmd_re.push_back("192k");
    }
    cmd_re.push_back("-map");
    cmd_re.push_back("[v]");
    if (map_audio) {
        cmd_re.push_back("-map");
        cmd_re.push_back("[a]");
    }
    cmd_re.push_back(reenc_temp);
    out_info(state, "Running ffmpeg (re-encode fallback)…");
    std::string enc_err;
    bool encoded = run_ffmpeg(state, cmd_re, enc_err) && file_size(reenc_temp) > 0;
    discard_file(list_path);
    if (encoded) {
        if (!promote_file(reenc_temp, out_path, err)) {
            discard_file(reenc_temp);
            out_err(state, "Job failed: " + err);
            return false;
        }
        out_info(state, "Done (with re-encode) → " + out_path);
        Job job;
        job.file = vid1;
        job.start = 0;
        job.end = 0;
        job.percent = 100;
        job.tail = "none";
        job.output = out_path;
        state.jobs.push_back(job);
        return true;
    }
    discard_file(reenc_temp);
    out_err(state, "ERROR: join failed on both stream-copy and re-encode paths.");
    if (!enc_err.empty()) {
        out_err(state, "   ffmpeg: " + enc_err);
    }
    remember(state, "join failed on both stream-copy and re-encode paths.", "");
    return false;
}

double probe_duration(State &state, const std::string &path)
{
    if (!on_path("ffprobe")) {
        out_err(state, "ERROR: ffprobe not found on PATH.");
        out_err(state, "   VideoTools reads duration with ffprobe from the FFmpeg package.");
        remember(state, "ffprobe not found on PATH.", "Install FFmpeg so ffprobe is on PATH.");
        return 0.0;
    }
    std::vector<std::string> cmd;
    cmd.push_back("ffprobe");
    cmd.push_back("-v");
    cmd.push_back("error");
    cmd.push_back("-show_entries");
    cmd.push_back("format=duration");
    cmd.push_back("-of");
    cmd.push_back("default=noprint_wrappers=1:nokey=1");
    cmd.push_back(path);
    std::string text;
    int code = host_spawn(cmd, text, HOST_CAPTURE_STDOUT);
    if (code != 0) {
        return 0.0;
    }
    char *end = NULL;
    double value = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || value <= 0.0) {
        return 0.0;
    }
    return value;
}

bool probe_text_has_token(const std::string &text)
{
    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char ch = static_cast<unsigned char>(text[i]);
        if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t') {
            return true;
        }
    }
    return false;
}

AudioProbe probe_audio(const std::string &path)
{
    if (!on_path("ffprobe")) {
        return AUDIO_UNKNOWN;
    }
    std::vector<std::string> cmd;
    cmd.push_back("ffprobe");
    cmd.push_back("-v");
    cmd.push_back("error");
    cmd.push_back("-select_streams");
    cmd.push_back("a");
    cmd.push_back("-show_entries");
    cmd.push_back("stream=index");
    cmd.push_back("-of");
    cmd.push_back("csv=p=0");
    cmd.push_back(path);
    std::string text;
    int code = host_spawn(cmd, text, HOST_CAPTURE_STDOUT);
    if (code != 0) {
        return AUDIO_UNKNOWN;
    }
    return probe_text_has_token(text) ? AUDIO_YES : AUDIO_NO;
}

std::string format_time(double seconds)
{
    if (seconds < 0) {
        seconds = 0;
    }
    int hours = static_cast<int>(seconds / 3600.0);
    int minutes = static_cast<int>(std::fmod(seconds, 3600.0) / 60.0);
    double secs = std::fmod(seconds, 60.0);
    char buf[64];
    if (hours == 0) {
        std::snprintf(buf, sizeof(buf), "%02d:%06.3f", minutes, secs);
    } else {
        std::snprintf(buf, sizeof(buf), "%02d:%02d:%06.3f", hours, minutes, secs);
    }
    return buf;
}

std::string atempo_chain(double speed)
{
    std::ostringstream filters;
    double remaining = speed;
    bool first = true;
    while (remaining > 100.0) {
        if (!first) {
            filters << ",";
        }
        filters << "atempo=100.0";
        first = false;
        remaining /= 100.0;
    }
    while (remaining < 0.5) {
        if (!first) {
            filters << ",";
        }
        filters << "atempo=0.5";
        first = false;
        remaining /= 0.5;
    }
    if (!first) {
        filters << ",";
    }
    char num[32];
    std::snprintf(num, sizeof(num), "%.6f", remaining);
    filters << "atempo=" << num;
    return filters.str();
}

bool parse_double(const std::string &text, double &value)
{
    if (text.empty()) {
        return false;
    }
    char *end = NULL;
    value = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0') {
        return false;
    }
    return true;
}

std::string build_edit_name(const std::string &video, double start, double end, double percent, const std::string &tail)
{
    char buf[512];
    std::snprintf(buf, sizeof(buf), "%s_cut%.1f-%.1fs_%dpct",
                  stem_of(video).c_str(), start, end, static_cast<int>(percent));
    std::string name = buf;
    if (tail == "boomerang") {
        name += "_BOOMERANG";
    } else if (tail == "repeat") {
        name += "_REPEAT";
    }
    name += ".mp4";
    return join_path(parent_dir(video), name);
}

bool concat_copy(State &state, const std::vector<std::string> &parts, const std::string &final_output)
{
    std::string err;
    std::string list_path = make_temp(final_output, ".txt", err);
    if (list_path.empty() || !write_concat_list(list_path, parts, err)) {
        discard_file(list_path);
        out_err(state, err.empty() ? "concat list failed" : err);
        return false;
    }
    std::vector<std::string> cmd;
    cmd.push_back("ffmpeg");
    cmd.push_back("-nostdin");
    cmd.push_back("-y");
    cmd.push_back("-hide_banner");
    cmd.push_back("-loglevel");
    cmd.push_back("error");
    cmd.push_back("-f");
    cmd.push_back("concat");
    cmd.push_back("-safe");
    cmd.push_back("0");
    cmd.push_back("-i");
    cmd.push_back(list_path);
    cmd.push_back("-c");
    cmd.push_back("copy");
    cmd.push_back(final_output);
    std::string run_err;
    bool ok = run_ffmpeg(state, cmd, run_err) && file_size(final_output) > 0;
    discard_file(list_path);
    if (!ok) {
#if defined(_WIN32)
        _unlink(final_output.c_str());
#else
        unlink(final_output.c_str());
#endif
        if (!run_err.empty()) {
            out_err(state, run_err);
        }
    }
    return ok;
}

bool edit_job(State &state, const std::string &video, double start, double end, double percent, const std::string &tail)
{
    state.mode = "edit";
    if (!is_regular_file(video)) {
        out_err(state, "ERROR: File not found: " + video);
        out_err(state, "   Next: pass an existing MP4 with --file.");
        remember(state, "File not found: " + video, "pass an existing MP4 with --file.");
        return false;
    }
    if (suffix_lower(video) != ".mp4") {
        out_err(state, "ERROR: edit accepts an MP4 file.");
        remember(state, "edit accepts an MP4 file.", "");
        return false;
    }
    if (percent < PERCENT_MIN || percent > PERCENT_MAX) {
        out_err(state, "ERROR: length percent must be from 20 to 200.");
        remember(state, "length percent must be from 20 to 200.", "");
        return false;
    }
    if (tail != "none" && tail != "boomerang" && tail != "repeat") {
        out_err(state, "ERROR: Choose none, boomerang, or repeat.");
        remember(state, "Choose none, boomerang, or repeat.", "");
        return false;
    }
    if (!ensure_ffmpeg(state)) {
        return false;
    }
    double duration = probe_duration(state, video);
    if (duration <= 0.0) {
        out_err(state, "ERROR: Could not read duration for " + base_name(video));
        remember(state, "Could not read duration for " + base_name(video), "");
        return false;
    }
    out_info(state, "Duration: " + format_time(duration));
    bool keep_audio = probe_audio(video) != AUDIO_NO;
    if (!(start >= 0.0 && start < end && end <= duration + 0.001)) {
        out_err(state, "ERROR: cut must satisfy 0 <= start < end <= duration.");
        remember(state, "cut must satisfy 0 <= start < end <= duration.", "");
        return false;
    }
    std::string final_out = build_edit_name(video, start, end, percent, tail);
    out_info(state, "Processing → " + final_out);
    std::string err;
    std::string cut_temp = make_temp(final_out, ".mp4", err);
    if (cut_temp.empty()) {
        out_err(state, err);
        return false;
    }
    char start_buf[64];
    char end_buf[64];
    std::snprintf(start_buf, sizeof(start_buf), "%.6f", start);
    std::snprintf(end_buf, sizeof(end_buf), "%.6f", end);
    std::string vf = std::string("trim=start=") + start_buf + ":end=" + end_buf + ",setpts=PTS-STARTPTS";
    std::string af = std::string("atrim=start=") + start_buf + ":end=" + end_buf + ",asetpts=PTS-STARTPTS";
    std::vector<std::string> cut_cmd;
    cut_cmd.push_back("ffmpeg");
    cut_cmd.push_back("-nostdin");
    cut_cmd.push_back("-y");
    cut_cmd.push_back("-hide_banner");
    cut_cmd.push_back("-loglevel");
    cut_cmd.push_back("error");
    cut_cmd.push_back("-i");
    cut_cmd.push_back(video);
    cut_cmd.push_back("-vf");
    cut_cmd.push_back(vf);
    if (keep_audio) {
        cut_cmd.push_back("-af");
        cut_cmd.push_back(af);
    }
    cut_cmd.push_back("-c:v");
    cut_cmd.push_back("libx264");
    cut_cmd.push_back("-preset");
    cut_cmd.push_back("ultrafast");
    cut_cmd.push_back("-crf");
    cut_cmd.push_back("17");
    if (keep_audio) {
        cut_cmd.push_back("-c:a");
        cut_cmd.push_back("aac");
    } else {
        cut_cmd.push_back("-an");
    }
    cut_cmd.push_back("-avoid_negative_ts");
    cut_cmd.push_back("make_zero");
    cut_cmd.push_back(cut_temp);
    out_info(state, "1. Cutting segment...");
    std::string run_err;
    if (!run_ffmpeg(state, cut_cmd, run_err) || file_size(cut_temp) <= 0) {
        discard_file(cut_temp);
        out_err(state, run_err.empty() ? "ERROR: ffmpeg failed on the cut." : run_err);
        remember(state, "ffmpeg failed on the cut.", "");
        return false;
    }
    std::string forward = cut_temp;
    std::string speed_temp;
    if (std::fabs(percent - 100.0) <= 1e-6) {
        out_info(state, "2. Length stays 100%...");
    } else {
        speed_temp = make_temp(final_out, ".mp4", err);
        if (speed_temp.empty()) {
            discard_file(cut_temp);
            out_err(state, err);
            return false;
        }
        double speed = 100.0 / percent;
        char pts[64];
        std::snprintf(pts, sizeof(pts), "%.6f", 1.0 / speed);
        std::string filter = std::string("[0:v]setpts=") + pts + "*PTS[v]";
        if (keep_audio) {
            filter += ";[0:a]" + atempo_chain(speed) + "[a]";
        }
        std::vector<std::string> speed_cmd;
        speed_cmd.push_back("ffmpeg");
        speed_cmd.push_back("-nostdin");
        speed_cmd.push_back("-y");
        speed_cmd.push_back("-hide_banner");
        speed_cmd.push_back("-loglevel");
        speed_cmd.push_back("error");
        speed_cmd.push_back("-i");
        speed_cmd.push_back(cut_temp);
        speed_cmd.push_back("-filter_complex");
        speed_cmd.push_back(filter);
        speed_cmd.push_back("-map");
        speed_cmd.push_back("[v]");
        if (keep_audio) {
            speed_cmd.push_back("-map");
            speed_cmd.push_back("[a]");
        }
        speed_cmd.push_back("-c:v");
        speed_cmd.push_back("libx264");
        speed_cmd.push_back("-preset");
        speed_cmd.push_back("medium");
        speed_cmd.push_back("-crf");
        speed_cmd.push_back("18");
        if (keep_audio) {
            speed_cmd.push_back("-c:a");
            speed_cmd.push_back("aac");
            speed_cmd.push_back("-b:a");
            speed_cmd.push_back("192k");
        } else {
            speed_cmd.push_back("-an");
        }
        speed_cmd.push_back("-movflags");
        speed_cmd.push_back("+faststart");
        speed_cmd.push_back(speed_temp);
        char shown[32];
        std::snprintf(shown, sizeof(shown), "%d", static_cast<int>(percent));
        out_info(state, std::string("2. Changing length to ") + shown + "%...");
        if (!run_ffmpeg(state, speed_cmd, run_err) || file_size(speed_temp) <= 0) {
            discard_file(cut_temp);
            discard_file(speed_temp);
            out_err(state, run_err.empty() ? "ERROR: ffmpeg failed on the speed change." : run_err);
            remember(state, "ffmpeg failed on the speed change.", "");
            return false;
        }
        forward = speed_temp;
    }
    bool published = false;
    if (tail == "boomerang") {
        out_info(state, "3. Creating boomerang...");
        std::string reverse_clip = make_temp(final_out, ".mp4", err);
        if (reverse_clip.empty()) {
            discard_file(cut_temp);
            discard_file(speed_temp);
            out_err(state, err);
            return false;
        }
        std::vector<std::string> rev;
        rev.push_back("ffmpeg");
        rev.push_back("-nostdin");
        rev.push_back("-y");
        rev.push_back("-hide_banner");
        rev.push_back("-loglevel");
        rev.push_back("error");
        rev.push_back("-i");
        rev.push_back(forward);
        rev.push_back("-vf");
        rev.push_back("reverse");
        if (keep_audio) {
            rev.push_back("-af");
            rev.push_back("areverse");
        } else {
            rev.push_back("-an");
        }
        rev.push_back(reverse_clip);
        if (!run_ffmpeg(state, rev, run_err) || file_size(reverse_clip) <= 0) {
            discard_file(reverse_clip);
            discard_file(cut_temp);
            discard_file(speed_temp);
            out_err(state, run_err.empty() ? "ERROR: ffmpeg failed on the reverse." : run_err);
            remember(state, "ffmpeg failed on the reverse.", "");
            return false;
        }
        std::vector<std::string> parts;
        parts.push_back(forward);
        parts.push_back(reverse_clip);
        published = concat_copy(state, parts, final_out);
        discard_file(reverse_clip);
    } else if (tail == "repeat") {
        out_info(state, "3. Repeating the clip...");
        FILE *in = std::fopen(forward.c_str(), "rb");
        std::string copy_clip = make_temp(final_out, ".mp4", err);
        FILE *out = copy_clip.empty() ? NULL : std::fopen(copy_clip.c_str(), "wb");
        bool copied = in != NULL && out != NULL;
        if (copied) {
            char buf[8192];
            size_t nread = 0;
            while ((nread = std::fread(buf, 1, sizeof(buf), in)) > 0) {
                if (std::fwrite(buf, 1, nread, out) != nread) {
                    copied = false;
                    break;
                }
            }
        }
        if (in) {
            std::fclose(in);
        }
        if (out) {
            std::fclose(out);
        }
        if (!copied) {
            discard_file(copy_clip);
            discard_file(cut_temp);
            discard_file(speed_temp);
            out_err(state, "ERROR: could not copy the clip for repeat.");
            return false;
        }
        std::vector<std::string> parts;
        parts.push_back(forward);
        parts.push_back(copy_clip);
        published = concat_copy(state, parts, final_out);
        discard_file(copy_clip);
    } else {
        if (!promote_file(forward, final_out, err)) {
            out_err(state, "Job failed: " + err);
            published = false;
        } else {
            published = true;
            if (forward == cut_temp) {
                cut_temp.clear();
            } else {
                speed_temp.clear();
            }
        }
    }
    discard_file(cut_temp);
    discard_file(speed_temp);
    if (!published) {
        remember(state, "edit failed.", "");
        if (state.error.empty()) {
            out_err(state, "ERROR: edit failed.");
        }
        return false;
    }
    out_info(state, "SUCCESS → " + base_name(final_out));
    Job job;
    job.file = video;
    job.start = start;
    job.end = end;
    job.percent = percent;
    job.tail = tail;
    job.output = final_out;
    state.jobs.push_back(job);
    return true;
}

void print_numbered(const std::vector<std::string> &files)
{
    for (size_t i = 0; i < files.size(); ++i) {
        std::printf("%zu. %s\n", i + 1, base_name(files[i]).c_str());
    }
}

bool pick_index(const std::vector<std::string> &files, const char *prompt, std::string &chosen)
{
    std::string line;
    if (!read_line(prompt, line)) {
        return false;
    }
    char *end = NULL;
    long value = std::strtol(line.c_str(), &end, 10);
    if (end == line.c_str() || *end != '\0' || value < 1 || static_cast<size_t>(value) > files.size()) {
        return false;
    }
    chosen = files[static_cast<size_t>(value - 1)];
    return true;
}

int cmd_list_videos(State &state)
{
    state.mode = "list-videos";
    std::vector<std::string> files = list_join_videos(".");
    state.files = files;
    if (!state.json) {
        for (size_t i = 0; i < files.size(); ++i) {
            std::puts(base_name(files[i]).c_str());
        }
    }
    return finish(state, 0);
}

int cmd_list_mp4(State &state, const std::string &folder)
{
    state.mode = "list-mp4";
    std::string root = folder.empty() ? "." : folder;
    if (!is_dir(root)) {
        out_err(state, "ERROR: folder not found: " + root);
        remember(state, "folder not found: " + root, "");
        return finish(state, 1);
    }
    std::vector<std::string> files = list_mp4(root);
    state.files = files;
    if (!state.json) {
        for (size_t i = 0; i < files.size(); ++i) {
            std::puts(files[i].c_str());
        }
    }
    return finish(state, 0);
}

int cmd_join(State &state, const std::string &first, const std::string &second, const std::string &output, bool interactive)
{
    state.mode = "join";
    std::string vid1 = first;
    std::string vid2 = second;
    std::string raw_name = output;
    if (vid1.empty() || vid2.empty()) {
        if (!interactive || !is_tty_in()) {
            out_err(state, "ERROR: join needs two videos. On a terminal it asks; otherwise pass --first and --second.");
            remember(state, "join needs two videos.", "Pass --first and --second.");
            return finish(state, 1);
        }
        std::vector<std::string> files = list_join_videos(".");
        if (files.size() < 2) {
            out_err(state, "Need at least 2 video files in this folder!");
            remember(state, "Need at least 2 video files in this folder!", "");
            return finish(state, 1);
        }
        print_numbered(files);
        if (!pick_index(files, "First video number: ", vid1)) {
            out_err(state, "ERROR: that first video is not on the list.");
            return finish(state, 1);
        }
        std::vector<std::string> rest;
        for (size_t i = 0; i < files.size(); ++i) {
            if (!same_file(files[i], vid1)) {
                rest.push_back(files[i]);
            }
        }
        print_numbered(rest);
        if (!pick_index(rest, "Second video number: ", vid2)) {
            out_err(state, "ERROR: that second video is not on the remaining list.");
            return finish(state, 1);
        }
        if (!read_line("Output name (empty uses the two file names): ", raw_name)) {
            raw_name.clear();
        }
    }
    std::string out_path = resolve_output_name(vid1, vid2, raw_name);
    bool ok = join_files(state, vid1, vid2, out_path);
    return finish(state, ok ? 0 : 1);
}

int cmd_edit(State &state, const std::string &file, const std::string &folder, const std::string &start_text,
             const std::string &end_text, const std::string &percent_text, const std::string &tail, bool interactive)
{
    state.mode = "edit";
    std::string video = file;
    std::string start_s = start_text;
    std::string end_s = end_text;
    std::string percent_s = percent_text.empty() ? "100" : percent_text;
    std::string chosen = tail.empty() ? "none" : tail;
    if (video.empty() || start_s.empty() || end_s.empty()) {
        if (!interactive || !is_tty_in()) {
            out_err(state, "ERROR: edit needs --file, --start, and --end.");
            remember(state, "edit needs --file, --start, and --end.", "");
            return finish(state, 1);
        }
        std::string root;
        if (!read_line("Folder (empty is this folder): ", root)) {
            return finish(state, 1);
        }
        if (root.empty()) {
            root = folder.empty() ? "." : folder;
        }
        if (!is_dir(root)) {
            out_err(state, "ERROR: folder not found: " + root);
            return finish(state, 1);
        }
        std::vector<std::string> files = list_mp4(root);
        if (files.empty()) {
            out_err(state, "ERROR: no MP4 files under that folder.");
            return finish(state, 1);
        }
        print_numbered(files);
        if (!pick_index(files, "MP4 number: ", video)) {
            out_err(state, "ERROR: that file is not on the list.");
            return finish(state, 1);
        }
        double duration = probe_duration(state, video);
        if (duration > 0.0) {
            out_info(state, "Duration: " + format_time(duration));
        }
        if (!read_line("Start seconds: ", start_s) || !read_line("End seconds: ", end_s)) {
            return finish(state, 1);
        }
        std::string percent_line;
        if (!read_line("Length percent (empty is 100, allowed 20-200): ", percent_line)) {
            return finish(state, 1);
        }
        if (!percent_line.empty()) {
            percent_s = percent_line;
        }
        std::string tail_line;
        if (!read_line("Tail none, boomerang, or repeat (empty is none): ", tail_line)) {
            return finish(state, 1);
        }
        if (!tail_line.empty()) {
            chosen = lower_copy(tail_line);
        }
    } else if (!folder.empty() && video.find('/') == std::string::npos && video.find('\\') == std::string::npos &&
               !(video.size() >= 2 && std::isalpha(static_cast<unsigned char>(video[0])) && video[1] == ':')) {
        video = join_path(folder, video);
    }
    double start = 0;
    double end = 0;
    double percent = 100;
    if (!parse_double(start_s, start) || !parse_double(end_s, end) || !parse_double(percent_s, percent)) {
        out_err(state, "ERROR: start, end, and percent must be numbers.");
        remember(state, "start, end, and percent must be numbers.", "");
        return finish(state, 1);
    }
    bool ok = edit_job(state, video, start, end, percent, chosen);
    return finish(state, ok ? 0 : 1);
}

void print_help(State &state)
{
    state.mode = "help";
    const char *lines[] = {
        "VideoTools 1.0.0 — join two local videos, and cut, speed, or boomerang an MP4.",
        "Join looks only in the current folder. It does not ask for a folder and it is not recursive.",
        "Join formats: .mp4 .mov .mkv .avi .m4v",
        "Join selection: first video, then the second from the list that remains.",
        "Join output: {name1} + {name2}.mp4 when the name is empty.",
        "list-videos lists those files and does not join.",
        "Join quality: stream copy first, then a re-encode if copy fails.",
        "Edit: folder, then one MP4. Cut start and end in seconds.",
        "Length percent defaults to 100 and must be from 20 to 200.",
        "Tail is none, boomerang, or repeat. Empty means none. The two tails cannot be combined.",
        "Edit input is MP4. A script passes edit --file, --start, and --end.",
        "list-mp4 lists MP4 files under a folder and does not encode.",
        "Edit output is written next to the source: {stem}_cut{start}-{end}s_{percent}pct[_BOOMERANG|_REPEAT].mp4",
        "Flags: --file --folder --start --end --percent --boomerang --repeat --first --second --output --json",
        "--json prints one object and does not open the menu.",
        "FFmpeg must be on PATH before a join or an encode. ffprobe reads duration.",
        "On a terminal with a display, VideoTools with no words opens the video window.",
        "With no display, or when standard output is not a terminal, that start shows the menu. Row 1 is join. Row 2 is edit. tui always shows the menu.",
        "Join does not cut, speed, or boomerang.",
        "log, luascript, and logcheck keep the gemini-wilgat shell. --gemini opens that shell. --window is an error.",
        "Examples:",
        "  VideoTools join --first a.mp4 --second b.mp4",
        "  VideoTools list-videos",
        "  VideoTools edit --file clip.mp4 --start 1 --end 5 --percent 50",
        "  VideoTools edit --file clip.mp4 --start 1 --end 5 --percent 50 --boomerang",
        "  VideoTools list-mp4",
        "  VideoTools about",
        NULL
    };
    if (state.json) {
        return;
    }
    for (size_t i = 0; lines[i] != NULL; ++i) {
        std::puts(lines[i]);
    }
}

void print_about(State &state)
{
    state.mode = "about";
    if (state.json) {
        return;
    }
    std::puts("VideoTools");
    std::printf("Version: %s\n", version_text().c_str());
    std::puts(PROJECT_DESCRIPTION);
    std::puts("Runtime tools: FFmpeg (copy, re-encode, cut, speed); ffprobe (duration).");
    std::puts("Entry: VideoTools");
    std::puts("A bare start on a terminal with a display opens the video window.");
    std::puts("Bootstrapped from gemini-wilgat. --gemini opens that shell.");
}

void print_version(State &state)
{
    state.mode = "version";
    if (state.json) {
        return;
    }
    std::printf("%s %s\n", PROJECT_NAME, version_text().c_str());
}

int cmd_menu(State &state)
{
    state.mode = "menu";
    if (!is_tty_in()) {
        print_help(state);
        return finish(state, 0);
    }
    for (;;) {
        std::puts("");
        std::puts("1. join        : pick two videos in this folder and concatenate");
        std::puts("2. edit        : cut, speed, and optional boomerang or repeat");
        std::puts("3. list-videos : names that join can use");
        std::puts("4. list-mp4    : MP4 names under a folder");
        std::puts("5. help");
        std::puts("6. about");
        std::puts("7. version");
        std::puts("9. Exit");
        std::string line;
        if (!read_line("> ", line)) {
            return finish(state, 0);
        }
        if (line == "1") {
            State child = state;
            child.json = false;
            cmd_join(child, "", "", "", true);
        } else if (line == "2") {
            State child = state;
            child.json = false;
            cmd_edit(child, "", "", "", "", "", "none", true);
        } else if (line == "3") {
            State child = state;
            child.json = false;
            cmd_list_videos(child);
        } else if (line == "4") {
            std::string folder;
            read_line("Folder (empty is this folder): ", folder);
            State child = state;
            child.json = false;
            cmd_list_mp4(child, folder);
        } else if (line == "5") {
            print_help(state);
        } else if (line == "6") {
            print_about(state);
        } else if (line == "7") {
            print_version(state);
        } else if (line == "9" || line == "q" || line == "quit" || line == "exit") {
            return finish(state, 0);
        } else if (!line.empty()) {
            std::fputs("That number is not on the menu.\n", stderr);
        }
    }
}

struct Parsed {
    std::string verb;
    std::string first;
    std::string second;
    std::string output;
    std::string file;
    std::string folder;
    std::string start;
    std::string end;
    std::string percent;
    bool boomerang;
    bool repeat;
    bool json;
    bool saw_tail;
    bool saw_percent;
    std::string bad;
    Parsed() : boomerang(false), repeat(false), json(false), saw_tail(false), saw_percent(false) {}
};

bool take_value(int argc, char **argv, int &i, std::string &dest, const char *flag)
{
    if (i + 1 >= argc) {
        dest = std::string("missing value for ") + flag;
        return false;
    }
    dest = argv[++i];
    return true;
}

Parsed parse_args(int argc, char **argv)
{
    Parsed parsed;
    std::vector<std::string> positionals;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--json") {
            parsed.json = true;
        } else if (arg == "--quiet") {
            continue;
        } else if (arg == "--boomerang") {
            parsed.boomerang = true;
            parsed.saw_tail = true;
        } else if (arg == "--repeat") {
            parsed.repeat = true;
            parsed.saw_tail = true;
        } else if (arg == "--first") {
            if (!take_value(argc, argv, i, parsed.first, "--first")) {
                parsed.bad = parsed.first;
            }
        } else if (arg == "--second") {
            if (!take_value(argc, argv, i, parsed.second, "--second")) {
                parsed.bad = parsed.second;
            }
        } else if (arg == "--output") {
            if (!take_value(argc, argv, i, parsed.output, "--output")) {
                parsed.bad = parsed.output;
            }
        } else if (arg == "--file") {
            if (!take_value(argc, argv, i, parsed.file, "--file")) {
                parsed.bad = parsed.file;
            }
        } else if (arg == "--folder") {
            if (!take_value(argc, argv, i, parsed.folder, "--folder")) {
                parsed.bad = parsed.folder;
            }
        } else if (arg == "--start") {
            if (!take_value(argc, argv, i, parsed.start, "--start")) {
                parsed.bad = parsed.start;
            }
        } else if (arg == "--end") {
            if (!take_value(argc, argv, i, parsed.end, "--end")) {
                parsed.bad = parsed.end;
            }
        } else if (arg == "--percent") {
            if (!take_value(argc, argv, i, parsed.percent, "--percent")) {
                parsed.bad = parsed.percent;
            }
            parsed.saw_percent = true;
        } else if (arg == "--help" || arg == "-h") {
            parsed.verb = "help";
        } else if (!arg.empty() && arg[0] == '-') {
            parsed.bad = "unknown flag " + arg;
        } else {
            positionals.push_back(arg);
        }
    }
    if (!parsed.bad.empty()) {
        return parsed;
    }
    if (!positionals.empty()) {
        const std::string &head = positionals[0];
        if (head == "help" || head == "version" || head == "about" || head == "join" ||
            head == "edit" || head == "list-videos" || head == "list-mp4" || head == "tui") {
            parsed.verb = head;
            if (head == "join") {
                if (positionals.size() >= 2 && parsed.first.empty()) {
                    parsed.first = positionals[1];
                }
                if (positionals.size() >= 3 && parsed.second.empty()) {
                    parsed.second = positionals[2];
                }
                if (positionals.size() >= 4 && parsed.output.empty()) {
                    parsed.output = positionals[3];
                }
                if (positionals.size() > 4) {
                    parsed.bad = "join accepts two videos and one output name";
                }
            } else if (positionals.size() > 1) {
                parsed.bad = "unexpected argument " + positionals[1];
            }
        } else {
            parsed.bad = "unknown command " + head;
        }
    }
    return parsed;
}

}  // namespace

bool video_domain_claims(int argc, char **argv)
{
    bool stdin_tty = is_tty_in();
    bool stdout_tty = is_tty_out();
    GuiRouteInput route_in = gui_route_from_argv(argc, argv, stdin_tty, stdout_tty);
    if (route_in.window_flag) {
        return true;
    }
    if (route_in.gemini) {
        return gui_route(route_in) != GUI_ROUTE_GEMINI;
    }
    std::string positional = gui_first_positional(argc, argv);
    if (positional == "log" || positional == "luascript" || positional == "logcheck") {
        return false;
    }
    return true;
}

int video_domain_run(int argc, char **argv)
{
    bool stdin_tty = is_tty_in();
    bool stdout_tty = is_tty_out();
    GuiRouteInput route_in = gui_route_from_argv(argc, argv, stdin_tty, stdout_tty);
    GuiRoute route = gui_route(route_in);
    Parsed parsed = parse_args(argc, argv);
    State state;
    state.json = parsed.json;
    if (route == GUI_ROUTE_ERROR) {
        std::string message = gui_route_error_text(route_in);
        out_err(state, message);
        remember(state, message, "");
        return finish(state, 1);
    }
    if (route == GUI_ROUTE_WINDOW) {
        return gui_window_open(argc, argv);
    }
    if (!parsed.bad.empty()) {
        out_err(state, "ERROR: " + parsed.bad);
        remember(state, parsed.bad, "VideoTools help");
        return finish(state, 1);
    }
    if (parsed.boomerang && parsed.repeat) {
        out_err(state, "ERROR: --boomerang and --repeat cannot be used together.");
        remember(state, "--boomerang and --repeat cannot be used together.", "");
        return finish(state, 1);
    }
    std::string tail = "none";
    if (parsed.boomerang) {
        tail = "boomerang";
    } else if (parsed.repeat) {
        tail = "repeat";
    }
    bool complete_edit = !parsed.file.empty() && !parsed.start.empty() && !parsed.end.empty();
    if (parsed.verb.empty() && complete_edit) {
        parsed.verb = "edit";
    }
    if (parsed.verb.empty() && (parsed.saw_percent || parsed.saw_tail) && !complete_edit) {
        out_err(state, "ERROR: --percent, --boomerang, or --repeat need --file, --start, and --end.");
        remember(state, "incomplete edit flags", "Pass --file, --start, and --end.");
        return finish(state, 1);
    }
    if (parsed.verb.empty() || parsed.verb == "tui") {
        if (parsed.verb == "tui" && !is_tty_in()) {
            out_err(state, "ERROR: tui needs a terminal.");
            remember(state, "tui needs a terminal.", "");
            return finish(state, 1);
        }
        return cmd_menu(state);
    }
    if (parsed.verb == "help") {
        print_help(state);
        return finish(state, 0);
    }
    if (parsed.verb == "version") {
        print_version(state);
        return finish(state, 0);
    }
    if (parsed.verb == "about") {
        print_about(state);
        return finish(state, 0);
    }
    if (parsed.verb == "list-videos") {
        return cmd_list_videos(state);
    }
    if (parsed.verb == "list-mp4") {
        return cmd_list_mp4(state, parsed.folder);
    }
    if (parsed.verb == "join") {
        bool interactive = parsed.first.empty() || parsed.second.empty();
        return cmd_join(state, parsed.first, parsed.second, parsed.output, interactive);
    }
    if (parsed.verb == "edit") {
        bool interactive = !complete_edit;
        return cmd_edit(state, parsed.file, parsed.folder, parsed.start, parsed.end, parsed.percent, tail, interactive);
    }
    out_err(state, "ERROR: unknown command");
    return finish(state, 1);
}

namespace {

std::string join_user_lines(const std::vector<std::string> &lines)
{
    std::string out;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i != 0) {
            out += "\n";
        }
        out += lines[i];
    }
    return out;
}

std::string working_dir()
{
#if defined(_WIN32)
    char buf[MAX_PATH];
    if (_getcwd(buf, MAX_PATH) == NULL) {
        return ".";
    }
    return buf;
#else
    char buf[4096];
    if (getcwd(buf, sizeof(buf)) == NULL) {
        return ".";
    }
    return buf;
#endif
}

bool has_separator(const std::string &path)
{
    return path.find('/') != std::string::npos || path.find('\\') != std::string::npos;
}

}  // namespace

VideoJobResult video_edit_encode(const std::string &video, double start, double end, double percent,
                                 const std::string &tail)
{
    State state;
    state.gui = true;
    VideoJobResult result;
    result.ok = edit_job(state, video, start, end, percent, tail);
    if (!state.jobs.empty()) {
        result.output_path = state.jobs.back().output;
    }
    result.error = state.error;
    if (result.ok) {
        result.user_text = "SUCCESS → " + result.output_path;
    } else {
        result.user_text = "Edit failed.";
    }
    return result;
}

VideoJobResult video_join_encode(const std::string &first, const std::string &second,
                                 const std::string &output_name)
{
    State state;
    state.gui = true;
    std::string output = resolve_output_name(first, second, output_name);
    if (!has_separator(output)) {
        output = join_path(working_dir(), output);
    }
    VideoJobResult result;
    result.ok = join_files(state, first, second, output);
    result.output_path = state.jobs.empty() ? output : state.jobs.back().output;
    result.error = state.error;
    if (!state.gui_lines.empty()) {
        result.user_text = join_user_lines(state.gui_lines);
    } else if (result.ok) {
        result.user_text = "SUCCESS";
    } else {
        result.user_text = "Join failed.";
    }
    return result;
}

bool video_probe_duration(const std::string &path, double &seconds, std::string &error)
{
    State state;
    state.gui = true;
    seconds = probe_duration(state, path);
    if (seconds <= 0.0) {
        error = state.error.empty() ? "Could not read duration for " + base_name(path) : state.error;
        return false;
    }
    error.clear();
    return true;
}

std::string video_edit_output_name(const std::string &video, double start, double end, double percent,
                                   const std::string &tail)
{
    return build_edit_name(video, start, end, percent, tail);
}

std::string video_join_output_name(const std::string &first, const std::string &second,
                                   const std::string &raw_name)
{
    std::string name = resolve_output_name(first, second, raw_name);
    if (!has_separator(name)) {
        name = join_path(working_dir(), name);
    }
    return name;
}

bool video_same_path(const std::string &a, const std::string &b)
{
    return same_file(a, b);
}

bool video_ffmpeg_available()
{
    return on_path("ffmpeg");
}
