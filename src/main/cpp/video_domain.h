// video_domain.h
// VideoTools domain entry. Specialized from the gemini-wilgat bootstrap.
// Join, edit, list, help, and about stop before the inherited wilgat window.

#ifndef VIDEO_DOMAIN_H
#define VIDEO_DOMAIN_H

// Returns false when this invocation belongs to the inherited wilgat shell
// (log, luascript, logcheck, or --gemini alone). --window is a VideoTools error.
// Every other start is VideoTools.
bool video_domain_claims(int argc, char **argv);

// Run the claimed VideoTools command. The return value is the process status.
int video_domain_run(int argc, char **argv);

#endif
