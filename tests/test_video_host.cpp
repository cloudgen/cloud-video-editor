#include "video_host.h"

#include <iostream>
#include <string>

static int g_failed = 0;

static void expect(bool ok, const std::string &name)
{
    if (!ok) {
        std::cerr << "FAIL " << name << "\n";
        g_failed += 1;
    }
}

int main()
{
    expect(host_basename("C:\\foo\\bar.mp4") == "bar.mp4", "win basename");
    expect(host_basename("/tmp/a.mp4") == "a.mp4", "posix basename");
    expect(host_parent_dir("C:\\foo\\bar.mp4") == "C:\\foo", "win parent");
    expect(host_parent_dir("/tmp/a.mp4") == "/tmp", "posix parent");
    std::string joined = host_join_path("folder", "clip.mp4");
#if defined(_WIN32)
    expect(joined == "folder\\clip.mp4", "win join");
#else
    expect(joined == "folder/clip.mp4", "posix join");
#endif
    expect(host_executable_on_path("ffmpeg"), "ffmpeg on PATH");
    if (g_failed != 0) {
        return 1;
    }
    std::cout << "video_host ok\n";
    return 0;
}
