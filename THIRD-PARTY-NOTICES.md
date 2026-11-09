# Third-party notices

**CVA-VEIL-Auto Sound Setting 1.3.0**, formerly TRUEITEM Audio Assistant,
is a modified version of **Loudness Balance Monitor**. This notice records the components used by this fork. It does not
replace individual license texts or provide a comprehensive legal clearance
for a public distribution.

## Loudness Balance Monitor

- Original author: **AllegroMoltoV**
- Upstream: <https://github.com/AllegroMoltoV/obs-loudness-balance-monitor>
- Based on: **v1.0.2**, commit
  `de4297086b28efdf5ac0990dd795cdb20b20d2c2`
- License declared by upstream: **GPL-2.0**
- License text: [LICENSE](LICENSE)
- Original author website: <https://www.allegromoltov.jp>

This is a TRUEITEM modification, not an official upstream release. Original
copyright and license notices are retained. Modifications made on 2026-11-09 are
described in [MODIFICATIONS.md](MODIFICATIONS.md). The modified program is
provided under GPL-2.0 with its corresponding source and build materials.

For a binary distribution, provide complete corresponding source for that exact
release, including local modifications, build/installation scripts, and required
non-system source components. Provide the source alongside the binary rather
than relying only on an upstream repository link. Retain release hashes and
build instructions so the source corresponds to the distributed binary.

## libebur128

- Project: <https://github.com/jiixyj/libebur128>
- Source version selected by the build: **v1.2.6**
- Copyright: **Copyright (c) 2011 Jan Kokemüller**
- License: **MIT**
- Use: loudness analysis; statically linked into the plugin.
- Complete notice: [third-party/libebur128-LICENSE.txt](third-party/libebur128-LICENSE.txt)
- Upstream notice: <https://github.com/jiixyj/libebur128/blob/v1.2.6/COPYING>

Keep the MIT copyright and permission notice in source and binary distribution
notices. The corresponding libebur128 source is included in
`third-party/libebur128`; a download reference alone is not described as the
complete source bundle for the combined plugin.

## OBS Studio

- Project: <https://github.com/obsproject/obs-studio>
- API/build and runtime-load reference: **33.0.1**.
- Upstream license: <https://github.com/obsproject/obs-studio/blob/33.0.1/COPYING>
- Use: OBS plugin, audio output, source capture, and frontend interfaces.

The plugin package does **not** bundle OBS Studio. It dynamically uses the
user's existing compatible OBS installation. Separately redistributing OBS
files requires their own corresponding notices and license compliance.

The SDK build-only change described in
[docs/portable-obs-sdk-patch.md](docs/portable-obs-sdk-patch.md) applies to the
extracted build dependency, not the installed OBS application. The original
OBS 33.0.1 source archive is preserved.

## Qt

- Project: <https://www.qt.io>
- UI modules used: **Qt Core, Qt Gui, Qt Widgets**, through the OBS-compatible
  Qt runtime. The local build/runtime-load check used **Qt 6.11.1**.
- Licensing information: <https://www.qt.io/licensing/open-source-lgpl-obligations>

The Qt 6.11.1 `include/QtWidgets/qwidget.h` and `include/QtCore/qobject.h` supplied
in the pinned official OBS Qt dependency both carry this SPDX expression:

```text
LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
```

Thus those inspected header notices explicitly include a GPL-2.0-only option;
they are not LGPL-only notices. This is evidence about the inspected headers,
not a declaration that every Qt module or redistribution scenario has received
a complete compatibility review. The Qt distribution's own copyright, license,
and third-party notices remain applicable.

The plugin package does **not** bundle Qt runtime DLLs. It dynamically uses the
compatible Qt runtime supplied by OBS. If a future package includes Qt DLLs or
other third-party binaries, identify the exact added files and satisfy their
applicable redistribution terms before shipping them. The absence of bundled
Qt DLLs alone does not complete that review.

## Windows graphical installer

The 1.3 installer includes CVA-authored PowerShell/WPF scripts and the small `SetupHost.cs` GUI host, with `Build-SetupHost.ps1` as its build entry. These modification sources retain GPL-2.0 notices and are included in the corresponding source materials.

The host and scripts use the user's existing Windows .NET Framework, Windows PowerShell 5.1/System.Management.Automation, WPF, and Windows Forms components. The plugin package does not redistribute those system runtimes or the C# compiler. Runtime requirements and packaging inventories must be checked for each final Windows bundle. Source availability alone is not a complete review of every redistribution scenario.

## Build and verification scope

The **1.3.0** Windows x64 DLL build and **five CTest tests pass**. The local development tool/SDK cache was copied into the new development folder; it does not depend on the old workspace path. The cache-reuse adapter and its build controls are included as public source. The normal bootstrap route and optional cache reuse are distinguished in [tools/README.md](tools/README.md).

The installer engine has **10 passing isolated synthetic-file tests on PowerShell 5.1 and 7**, including replacement failure and rollback. The native host's actual window was observed switching Korean/English/Japanese and blocking installation while OBS is open. GUI-launched end-to-end installation, real UAC interaction, and final bootstrap-error handling remain separate checks.

The **1.3 channel/MKV fixture passes** in isolated portable OBS 33.0.1: 16 GUI-thread public-slot steps, four actual MKVs, all encoded AAC streams decoded, and audio-stream counts **1 / 3 / 3 / 1**. It verifies music inclusion/exclusion from every selected editing track, voice-only recording, preview/apply, and exact undo. An observed three-to-one stale-track failure was fixed using the official idle output API and the same four cases passed again. Earlier import and encoded-count failures remain preserved. The final normal shutdown reported zero memory leaks; it is not a general leak audit.

Separate actual-window observation in a dedicated portable OBS verified the Korean connection form, all five input rows, the final routing preview, cancellation before apply, and main-screen Korean/English/Japanese changes. This is isolated-window evidence, not production OBS visual verification or an audit of every translated dialog state.

Production installation and startup readback also **pass**: the common installer engine installed and verified five files, their hashes match the tested build, and the restarted OBS log reports a successful 1.3.0 plugin load. Existing voice/game/music/stream-listening audio fields and output mode/track assignments are unchanged in before/after comparisons. This confirms installation, file integrity, startup, and settings preservation; it is not production-window visual evidence.

The verified DLL SHA256 is `5222fc14648f45ecdef110e4fb6c22c53f1e6bcb2e618324ab93c45433fd1a39`. The recording fixture used synthetic tones and the Advanced Standard MKV path. Full physical input, real new hardware/app selection or creation, save-failure injection, Simple-output/replay-buffer recording, live streaming, complete language flows, and final personal listening remain unverified. Production OBS visual verification remains **UNVERIFIED**.

The old 1.2.0 strict music-only recording-silence result remains **raw FAIL / INCONCLUSIVE** in historical records. Its possible transition-buffer explanation was not proven. It is not evidence that the new 1.3 channel configuration passed. Full keyboard/screen-reader use and final listening remain unverified.

These technical results are not a comprehensive legal approval, a universal compatibility claim, or a public-upload record. Individual evidence scopes and remaining limits must accompany final distribution claims.

Compiler, Windows SDK, CMake, and downloaded development dependencies are build tools, not components to copy wholesale into a plugin package. Their versions and build entry points are recorded in [tools/README.md](tools/README.md). Provide the exact corresponding modified source, build/installation scripts, and required component notices with each Windows archive.

## Design concept references

The 1.2.0 UI information hierarchy refers to
[Dark Mode Music Studio UI Design — Kron Raufi / Loka, Inc.](https://dribbble.com/shots/26615537-Dark-Mode-Music-Studio-UI-Design)
and [Mobile Dashboard w/ Real Data — Jang Trịnh](https://www.behance.net/gallery/51204899/Mobile-Dashboard-w-Real-Data-Dark-theme-UI).
These are credited concept references, not bundled artwork/code, endorsements,
or evidence that the modified product has passed usability testing. No private
OBS/BRIDGE settings or user-specific volume values belong in the public package.
