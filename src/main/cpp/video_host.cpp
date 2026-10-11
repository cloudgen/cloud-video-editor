#include "video_host.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

bool is_sep(char c)
{
    return c == '/' || c == '\\';
}

char native_sep()
{
#if defined(_WIN32)
    return '\\';
#else
    return '/';
#endif
}

#if !defined(_WIN32)
int spawn_posix(const std::vector<std::string> &args, std::string &captured, HostCapture which)
{
    int pipefd[2];
    if (pipe(pipefd) != 0) {
        captured = "pipe failed";
        return 127;
    }
    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        captured = "fork failed";
        return 127;
    }
    if (pid == 0) {
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            if (which == HOST_CAPTURE_STDERR) {
                dup2(devnull, STDOUT_FILENO);
            } else {
                dup2(devnull, STDERR_FILENO);
            }
            if (devnull > 2) {
                close(devnull);
            }
        }
        dup2(pipefd[1], which == HOST_CAPTURE_STDERR ? STDERR_FILENO : STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        std::vector<char *> argv;
        argv.reserve(args.size() + 1);
        for (size_t i = 0; i < args.size(); ++i) {
            argv.push_back(const_cast<char *>(args[i].c_str()));
        }
        argv.push_back(NULL);
        execvp(argv[0], argv.data());
        _exit(127);
    }
    close(pipefd[1]);
    std::string text;
    char buf[512];
    ssize_t nread = 0;
    while ((nread = read(pipefd[0], buf, sizeof(buf))) > 0) {
        text.append(buf, buf + nread);
        if (text.size() > 8192) {
            text.erase(0, text.size() - 4096);
        }
    }
    close(pipefd[0]);
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        captured = "wait failed";
        return 127;
    }
    captured = text;
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    captured = "ffmpeg did not exit";
    return 1;
}
#else
std::wstring widen(const std::string &text)
{
    if (text.empty()) {
        return std::wstring();
    }
    int n = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, NULL, 0);
    if (n <= 0) {
        return std::wstring();
    }
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &out[0], n);
    if (!out.empty() && out[out.size() - 1] == L'\0') {
        out.erase(out.size() - 1);
    }
    return out;
}

std::string quote_arg(const std::string &arg)
{
    std::string out = "\"";
    for (size_t i = 0; i < arg.size(); ++i) {
        if (arg[i] == '"') {
            out += "\\\"";
        } else {
            out += arg[i];
        }
    }
    out += "\"";
    return out;
}

int spawn_windows(const std::vector<std::string> &args, std::string &captured, HostCapture which)
{
    std::string line;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i) {
            line += " ";
        }
        line += quote_arg(args[i]);
    }
    SECURITY_ATTRIBUTES sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE read_pipe = NULL;
    HANDLE write_pipe = NULL;
    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
        captured = "pipe failed";
        return 127;
    }
    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);
    HANDLE nul = CreateFileW(L"NUL", GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
    STARTUPINFOW si;
    std::memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = nul;
    if (which == HOST_CAPTURE_STDERR) {
        si.hStdOutput = nul;
        si.hStdError = write_pipe;
    } else {
        si.hStdOutput = write_pipe;
        si.hStdError = nul;
    }
    std::wstring wide = widen(line);
    std::vector<wchar_t> mutable_line(wide.begin(), wide.end());
    mutable_line.push_back(L'\0');
    PROCESS_INFORMATION pi;
    std::memset(&pi, 0, sizeof(pi));
    BOOL ok = CreateProcessW(NULL, mutable_line.data(), NULL, NULL, TRUE,
                             CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    if (write_pipe != NULL) {
        CloseHandle(write_pipe);
    }
    if (nul != NULL && nul != INVALID_HANDLE_VALUE) {
        CloseHandle(nul);
    }
    if (!ok) {
        CloseHandle(read_pipe);
        captured = "CreateProcessW failed";
        return 127;
    }
    std::string text;
    char buf[512];
    DWORD nread = 0;
    while (ReadFile(read_pipe, buf, sizeof(buf), &nread, NULL) && nread > 0) {
        text.append(buf, buf + nread);
        if (text.size() > 8192) {
            text.erase(0, text.size() - 4096);
        }
    }
    CloseHandle(read_pipe);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    captured = text;
    return static_cast<int>(code);
}
#endif

}  // namespace

int host_spawn(const std::vector<std::string> &args, std::string &captured, HostCapture which)
{
    if (args.empty()) {
        captured = "empty command";
        return 127;
    }
#if defined(_WIN32)
    return spawn_windows(args, captured, which);
#else
    return spawn_posix(args, captured, which);
#endif
}

std::string host_basename(const std::string &path)
{
    size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) {
        return path;
    }
    return path.substr(slash + 1);
}

std::string host_parent_dir(const std::string &path)
{
    size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) {
        return ".";
    }
    if (slash == 0) {
        return std::string(1, path[0]);
    }
    return path.substr(0, slash);
}

std::string host_join_path(const std::string &dir, const std::string &name)
{
    if (dir.empty() || dir == ".") {
        return name;
    }
    if (is_sep(dir[dir.size() - 1])) {
        return dir + name;
    }
    return dir + native_sep() + name;
}

std::string host_real_path(const std::string &path)
{
#if defined(_WIN32)
    char buf[MAX_PATH];
    if (_fullpath(buf, path.c_str(), MAX_PATH) == NULL) {
        return "";
    }
    return buf;
#else
    char buf[4096];
    if (realpath(path.c_str(), buf) == NULL) {
        return "";
    }
    return buf;
#endif
}

bool host_executable_on_path(const char *name)
{
    const char *path = std::getenv("PATH");
    if (path == NULL || name == NULL || name[0] == '\0') {
        return false;
    }
#if defined(_WIN32)
    const char sep = ';';
#else
    const char sep = ':';
#endif
    std::string text(path);
    size_t start = 0;
    while (start <= text.size()) {
        size_t cut = text.find(sep, start);
        std::string dir = text.substr(start, cut == std::string::npos ? std::string::npos : cut - start);
        if (dir.empty()) {
            dir = ".";
        }
        std::string full = host_join_path(dir, name);
#if defined(_WIN32)
        std::string with_exe = full;
        if (full.size() < 4 || full.substr(full.size() - 4) != ".exe") {
            with_exe = full + ".exe";
        }
        if (_access(full.c_str(), 0) == 0 || _access(with_exe.c_str(), 0) == 0) {
            return true;
        }
#else
        if (access(full.c_str(), X_OK) == 0) {
            return true;
        }
#endif
        if (cut == std::string::npos) {
            break;
        }
        start = cut + 1;
    }
    return false;
}

std::string host_temp_dir()
{
#if defined(_WIN32)
    char buf[MAX_PATH];
    DWORD n = GetTempPathA(MAX_PATH, buf);
    if (n == 0 || n >= MAX_PATH) {
        return ".";
    }
    return std::string(buf);
#else
    const char *tmp = std::getenv("TMPDIR");
    if (tmp != NULL && tmp[0] != '\0') {
        return tmp;
    }
    return "/tmp";
#endif
}

std::string host_make_temp(const std::string &folder, const char *suffix, std::string &err)
{
#if defined(_WIN32)
    char dir[MAX_PATH];
    std::snprintf(dir, sizeof(dir), "%s", folder.c_str());
    char prefix[] = "vtl";
    char path[MAX_PATH];
    if (GetTempFileNameA(dir, prefix, 0, path) == 0) {
        err = "could not create a unique temp";
        return "";
    }
    std::string out(path);
    if (suffix != NULL && suffix[0] != '\0') {
        std::string with = out + suffix;
        if (!MoveFileA(out.c_str(), with.c_str())) {
            DeleteFileA(out.c_str());
            err = "could not name the temp file";
            return "";
        }
        out = with;
    }
    return out;
#else
    std::string pattern = host_join_path(folder, "videotoolsXXXXXX");
    std::vector<char> tmpl(pattern.begin(), pattern.end());
    tmpl.push_back('\0');
    int fd = mkstemp(tmpl.data());
    if (fd < 0) {
        err = "could not create a unique temp";
        return "";
    }
    close(fd);
    std::string path(tmpl.data());
    if (suffix != NULL && suffix[0] != '\0') {
        std::string with = path + suffix;
        if (rename(path.c_str(), with.c_str()) != 0) {
            unlink(path.c_str());
            err = "could not name the temp file";
            return "";
        }
        path = with;
    }
    return path;
#endif
}
