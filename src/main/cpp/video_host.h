// video_host.h
// Process and path helpers. Linux uses fork/exec. Windows uses CreateProcessW.

#ifndef VIDEO_HOST_H
#define VIDEO_HOST_H

#include <string>
#include <vector>

enum HostCapture {
    HOST_CAPTURE_STDERR = 0,
    HOST_CAPTURE_STDOUT = 1
};

// Run argv without a shell. Returns the child exit code, or 127 when spawn fails.
// captured receives the selected stream. On STDERR it is the tail used by ffmpeg errors.
int host_spawn(const std::vector<std::string> &args, std::string &captured, HostCapture which);

std::string host_basename(const std::string &path);
std::string host_parent_dir(const std::string &path);
std::string host_join_path(const std::string &dir, const std::string &name);
std::string host_real_path(const std::string &path);
bool host_executable_on_path(const char *name);
std::string host_temp_dir();
std::string host_make_temp(const std::string &folder, const char *suffix, std::string &err);

#endif
