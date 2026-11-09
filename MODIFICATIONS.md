# CVA-VEIL-Auto Sound Setting modifications

Modification date: **2026-11-09, Asia/Seoul**. Current revision: **1.3.0**. Build/core tests, the isolated actual-OBS channel/MKV fixture, and the installer language/OBS-open state are verified within the scopes below.

Project: **CVA-VEIL-Auto Sound Setting**, formerly TRUEITEM Audio Assistant, a fork of AllegroMoltoV's Loudness Balance
Monitor **v1.0.2**, upstream commit
`de4297086b28efdf5ac0990dd795cdb20b20d2c2`.

The upstream GPL-2.0 license and original attribution are retained. This is a
TRUEITEM modification, not an official upstream release or a claim that a public
release has been uploaded.

## 1.3.0 initial sound setup, recording choices, languages, and installation

- Add a three-step **Sound setup** flow: connect voice/game/music and the stream-listening app; choose live-stream/recording inclusion for every audio input; review the actual resulting lists before applying.
- List remaining inputs as **여타 소리** (Other sounds). Multiple other inputs can be assigned. They are routing targets, not automatic level-calibration targets.
- Enumerate all audio inputs, including global sources. Build the preview from every resulting mask so unassigned alerts or desktop captures cannot silently disappear from the displayed recording inventory.
- Exclude an unchecked sound from every selected recording track. Include a checked sound in the main recording mix while preserving its existing dedicated editing assignments. Preserve the service VOD assignment when preparing distinct recording tracks.
- Keep output mode, video quality, encoder choices, and the selected stream track unchanged. In supported general recording modes, update only the selected `RecTracks`; next-recording behavior is verified for the Advanced Standard MKV path described below.
- Reject unsupported/shared-stream-quality, lossless, custom FFmpeg, or unsupported container paths with a preparation message instead of reporting a successful split.
- Add microphone/device or application-capture drafts. Create them only at final apply; cancellation leaves existing OBS sources untouched. Device/app options come from OBS properties.
- Journal profile, scene collection/current scene, output signature, recording masks, assistant choices, and source UUID/before/after masks. Check current state before apply/restore; a failed journal write changes no routing. Existing source gains and processing filters are preserved.
- Restore previous channel routing separately from volume Undo. Newly created inputs are retained with their original zero routing; they are not deleted after users may have reused them elsewhere.
- Make game/music optional throughout checking, proposals, applying, reports, and 1–3-input volume Undo. Voice remains required. Remove fixed “music excluded is correct” route tests and translated-sentence state comparisons.
- Default to Korean with English/Japanese selection. Localize core/static/dynamic messages, setup, tooltips, and accessibility names through `data/i18n.json`; fall back to Korean copy. Keep user-entered/device/app names unchanged.
- Provide a Windows graphical setup flow, a console-free installer host, and a shared transactional engine. Keep the command-line preview. Verify files before replacement, back up old files in a writable per-user location, block while OBS is running, and restore replaced files on failure. Installer source/build scripts accompany the binary.
- Keep plugin/module/configuration names compatible with previous versions. Public packages exclude personal OBS/BRIDGE settings, authentication, private URLs, and test audio.

The runtime wording does not promise to separate sounds already mixed by a device or a browser process. Recording choices affect local recordings, not a service's server-side replay policy.

## 1.2.0 branding and UI refactor

- Use the formal visible name **CVA-VEIL-Auto Sound Setting**. Keep the existing
  internal module/configuration names for compatibility.
- Focus the main screen on three aligned microphone/game/music status rows with
  level bars and one primary 30-second check action.
- Present the 25-second speech and final 5-second quiet phases with a large
  seconds countdown and direct instructions.
- Move professional measurements, source selectors, output routing, and working
  references into **세부 정보**; provide terminology and flow guidance through
  **도움말**. Both entry points and primary controls are designed for keyboard access.
- This revision reorganizes UI/branding; it does not change the audio-processing
  policy, calibration bounds, or saved user gains. Personal PC settings are not
  part of the public source/plugin package.
- Design references: [Kron Raufi / Loka, Inc., Dribbble](https://dribbble.com/shots/26615537-Dark-Mode-Music-Studio-UI-Design)
  for consistent aligned audio rows/color distinction, and
  [Jang Trịnh, Behance](https://www.behance.net/gallery/51204899/Mobile-Dashboard-w-Real-Data-Dark-theme-UI)
  for large important-status hierarchy. These are concept references, not
  evidence of tested product usability. Artwork/code/decorative charts are not
  copied. The modified OBS screen needs its own rendered/keyboard verification.

The following measurement and bounded-calibration capabilities originate in the
1.1.0 modification and are retained by this UI revision.

## Measurement and diagnosis

- Observe the actual OBS broadcast and local recording output tracks separately
  during a finite 30-second check.
- Preserve stereo audio for LUFS/sample-peak measurement. Do not interpret a mono
  sum of independently delivered source callbacks as the final OBS stereo mix.
- Show microphone, game, and music diagnostics separately from output-track
  measurements and explain their different measurement scopes.
- Explain missing input, insufficient speech, waiting for speech, low level, and
  unresolved headroom conditions instead of labelling quiet intervals as a
  platform-specification failure.
- Diagnose source-to-track assignments, including microphone omission and music
  unexpectedly included in selected local recording tracks.

## Bounded calibration

- The check asks for ordinary speech/game/music for the first 25 seconds and
  a quiet microphone for the last 5 seconds of its 30-second interval.
- Initial configurable working references are voice −20 LUFS, game 6 LU below
  the proposed voice level, and music 12 LU below it. These are not CHZZK rules.
- Require sufficient elapsed time and valid speech. Quiet/noise evidence is
  required before suggesting a voice increase.
- Limit voice changes to ±3dB. The integrated UI limits game/music reductions
  to 6dB and never proposes automatically boosting those background sources.
- Use a default per-source sample-peak reference of −3dBFS. Also constrain any
  proposed voice boost to the observed whole-output sample-peak headroom below
  −1dBFS. Missing whole-output peak evidence does not permit a boost.
- The observed-output headroom guard is not a True Peak calculation or a
  mathematical guarantee about future/post-change mixtures. Program material,
  phase, overlap, and dynamics can change; remeasure after adjustment.
- Apply only after representative-input confirmation and while streaming,
  recording, and replay-buffer outputs are idle. Checking alone changes no gain.
- Persist previous values before modifying source volume. Apply/undo validation
  checks source identity and blocks overwriting later user volume changes.

## Measurement lifecycle and saved data

- Connect actual output-track observation only for the finite check and
  disconnect it on completion/cancellation. Source diagnostics are separate.
- Measurement reports contain numerical summaries and relevant non-secret
  source/routing/settings facts. The new check does not save raw audio.
- Save assistant settings, last measurement, and the last adjustment/undo state
  in the plugin configuration folder. Reports are not whole-device backups.

## Portable build adaptations

Windows x64 can be built with a workspace-local MSVC/SDK toolchain and Ninja
Multi-Config without a Visual Studio installation. Bootstrap adjustments only
supply Visual Studio architecture arguments to Visual Studio generators and
use a compiler-confirmed x64 fallback for the supported Ninja dependency assets.

The extracted OBS SDK source receives a documented, idempotent gate that skips
unused Win32 capture/injection helpers when plugins are disabled. This is a
build-dependency-only adaptation: the original OBS source ZIP and installed OBS
application are unchanged. See [docs/portable-obs-sdk-patch.md](docs/portable-obs-sdk-patch.md)
and [tools/README.md](tools/README.md). This tested Windows x64 path is not a claim
that every generator, architecture, or future dependency release is supported.

## Verification status

**1.3.0 status, 2026-11-09**:

- The Windows x64 DLL builds in the new self-contained development folder. **5/5 CTest tests pass**: `preview-recording-core`, `preview-analysis-core`, `trueitem-calibration`, `trueitem-output-monitor-stereo`, and `cva-channel-routing`.
- The shared installer engine's **10 isolated tests pass on Windows PowerShell 5.1 and PowerShell 7**, including a mid-replacement failure and restoration. These synthetic-file tests do not prove native installer-window operation or real UAC interaction.
- **PASS: isolated portable OBS 33.0.1, 16 GUI-thread public-slot steps and four actual MKV recordings.** The fixture verifies preview/apply/exact undo, draft-choice cancellation, unsupported recording rejection, context drift rejection, music inclusion/exclusion across all selected editing tracks, and voice-only routing. All encoded AAC streams were decoded and checked against synthetic tones; the four audio-stream counts are **1 / 3 / 3 / 1**.
- An earlier actual run found that reducing three recording tracks to one left two silent stale streams in OBS 33.0.1. Clearing trailing recording encoders through the official idle output API fixed this; the same four cases passed after the fix. The initial import-error run and the encoded-stream-count failure remain preserved as historical raw results. The final OBS shutdown reported **0 memory leaks**; this is not a general product leak audit.
- The verified DLL SHA256 is `5222fc14648f45ecdef110e4fb6c22c53f1e6bcb2e618324ab93c45433fd1a39`. Local evidence is `fixture-summary.json`, `fixture-result.json`, and four case-specific `*-encoded-analysis.json` files in the isolated fixture work area. The final summary and encoded analyses supersede the raw result's earlier decode-pending stage; historical failures are not rewritten.
- **PASS: separate dedicated portable OBS visual observation**, recorded in `visual-1.3-observation.json`. The actual window showed the Korean connection form, all five routing rows including other audio and stream listening, and the final include/exclude preview. Choices were cancelled before applying. Main static/dynamic text changed to English and Japanese, then returned to Korean. This is isolated-window evidence, not production OBS visual verification or a complete audit of every translated state.
- The console-free Windows setup executable's actual WPF window was observed switching Korean/English/Japanese and blocking installation while OBS is open. GUI-launched end-to-end installation, real UAC interaction, and final bootstrap-error handling remain separate checks.
- **PASS: production installation and startup readback.** The common installer engine installed and verified five files. Installed hashes match the verified build, and the restarted OBS log reports successful plugin load at version 1.3.0. Before/after fingerprints of the existing voice/game/music/stream-listening audio fields and output mode/track assignments are unchanged. Evidence is `production-1.3-install-receipt.json` and `production-1.3-observation.json`; no personal values are included here.
- Full physical keyboard/mouse and screen-reader use, real device/app creation or selection, save-failure injection, Simple-output/replay-buffer recording, live streaming, complete language flows, a broad OS/OBS compatibility matrix, and final subjective listening remain **UNVERIFIED**. The recording fixture used synthetic tones and public Qt slots, not actual microphone/app capture. **Production OBS visual verification remains UNVERIFIED**; the separate fixture's visual PASS does not replace it.

### Historical evidence is separate

Version **1.2.0 B319** built and passed its four core tests. Its production OBS display and isolated countdown/apply/undo/dialog/initial-geometry observations were recorded separately. Its strict music-only accumulated-silence assertion was **raw FAIL / INCONCLUSIVE**: the final recording Short-term LUFS window was null, but a finite cumulative peak remained after an immediate fader-zero transition. Residual buffered game audio was an inference, not a waveform-confirmed cause. That failure is preserved and is not substituted with an overall PASS or used as proof of the new 1.3 channel feature.

Earlier 1.1 observations and this 1.2 history are not evidence for the completed 1.3 channel/MKV fixture. Build/core checks, isolated fixtures, production rendering, native input, installer rollback, and final listening are distinct evidence scopes. Current release claims must follow the matching 1.3 result records.

Sample Peak is supported; **True Peak is not claimed**. LUFS references and bounded gain proposals do not guarantee subjective sound quality or future clipping immunity.

See [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) for redistribution notices and [docs/TRUEITEM-사용안내.md](docs/TRUEITEM-사용안내.md) for the user flow.
