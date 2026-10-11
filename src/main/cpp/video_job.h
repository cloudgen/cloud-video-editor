// video_job.h
// Encode entry points the video window calls. The CLI keeps the same jobs.

#ifndef VIDEO_JOB_H
#define VIDEO_JOB_H

#include <string>

struct VideoJobResult {
    bool ok;
    std::string output_path;
    std::string error;
    std::string user_text;

    VideoJobResult() : ok(false) {}
};

VideoJobResult video_edit_encode(const std::string &video, double start, double end,
                                 double percent, const std::string &tail);
VideoJobResult video_join_encode(const std::string &first, const std::string &second,
                                 const std::string &output_name);
bool video_probe_duration(const std::string &path, double &seconds, std::string &error);
std::string video_edit_output_name(const std::string &video, double start, double end,
                                   double percent, const std::string &tail);
std::string video_join_output_name(const std::string &first, const std::string &second,
                                   const std::string &raw_name);
bool video_same_path(const std::string &a, const std::string &b);
bool video_ffmpeg_available();

#endif
