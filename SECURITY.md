# Security Policy

VideoTools 1.0.0 joins two local videos and cuts, speeds, or boomerangs an MP4. It also opens a picture window for those jobs. `--gemini`, `log`, `luascript`, and `logcheck` open the inherited WebKit shell.

## Supported Versions

| Version | Supported |
|---------|-----------|
| 1.0.0 (current) | Yes |
| Older releases | None. This is the first public release. |

## Reporting a Vulnerability

Please do not open a public issue for a security-sensitive report when a private channel is available.

**Maintainer contact (email):** `cloudgen.wong@gmail.com`

- Source of contact: product author-email in [`LICENSE.md`](./LICENSE.md) (Copyright line). This file displays that address. It does not replace it.
- Prefer email for the vulnerability, the steps that show it, and the impact.
- You should receive an acknowledgment when the report is received and actionable.
- A private GitHub advisory for this repository is [github.com/cloudgen/cloud-video-editor/security/advisories/new](https://github.com/cloudgen/cloud-video-editor/security/advisories/new).
- Do not include exploit weaponization guides in public channels.

## Security Design Principles (CIAO)

This project follows **[CIAO](https://github.com/cloudgen/ciao)** / **CIAO-Lite** defensive design. Security-relevant intent:

| Letter | Principle | Security application |
|--------|-----------|----------------------|
| **C** | **Caution** | Treat paths, percents, and the display as boundaries. A refused display stops. An incomplete edit stops. `ffmpeg` is started without a shell. |
| **I** | **Intentional** | The picture window, the menu, and the inherited shell are separate starts. `--window` is an error. A normal user runs the program. It does not install packages. |
| **A** | **Anti-fragile** | Missing `libmpv` shows the playback sentence and leaves encode available. A video-only file omits audio filters instead of failing the job. |
| **O** | **Over-protect** | The picture thread does not wait on the player client. Closing during Run waits until the job finishes, then unrealizes the panes. |

Full principles: [CIAO Defensive Programming](https://github.com/cloudgen/ciao) · agent contract: [CIAO-Lite](https://github.com/cloudgen/ciao-lite).

This section describes design posture. It is not a claim of third-party certification.

## Scope

In scope:

- Local join and edit, including the files `ffmpeg` and `ffprobe` read and write
- The picture window and the playback library it loads at runtime
- The inherited WebKit shell opened only by `--gemini`, `log`, `luascript`, or `logcheck`
- Persistence under `~/.app/VideoTools/` (`PROJECT_ID` in `src/main/cpp/project.h`)

Out of scope:

- Vulnerabilities in upstream GTK, WebKitGTK, FFmpeg, or libmpv. Report those to the projects that ship them.
- Web-origin issues on sites the shell loads, including gemini.google.com
- This tree has no online installer and no companion checksum. There is no download-integrity claim.

## Scope notes

- Preferred language for reports: English.
- Out of scope: social engineering of third parties, physical attacks, and spam.
- Related docs: [`README.md`](./README.md), [`LICENSE.md`](./LICENSE.md).
