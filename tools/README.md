# Building CVA-VEIL-Auto Sound Setting 1.3.0

The current Windows x64 DLL builds in the self-contained development folder,
and **5/5 CTest tests pass**: `preview-recording-core`,
`preview-analysis-core`, `trueitem-calibration`,
`trueitem-output-monitor-stereo`, and `cva-channel-routing`. These tests cover
core measurement and routing policy. A separate **1.3 actual-OBS fixture also
passes**: 16 GUI-thread public-slot steps and four recorded MKVs, with every
encoded AAC stream independently decoded and checked against synthetic tones.
The audio-stream counts are **1 / 3 / 3 / 1**. Music inclusion, exclusion from
all selected editing tracks, voice-only output, preview/apply, and exact undo
passed on the Advanced Standard MKV path.

The verified DLL SHA256 is
`5222fc14648f45ecdef110e4fb6c22c53f1e6bcb2e618324ab93c45433fd1a39`.
The initial import-error run and a three-to-one stale encoded-track failure are
preserved in the fixture history. After clearing trailing recording encoders
through the official idle output API, the same four cases passed again. The
final normal OBS shutdown reported **0 memory leaks**; this does not establish
a general product leak audit. The final `fixture-summary.json` and four
case-specific `*-encoded-analysis.json` files contain the decoded result; the
raw `fixture-result.json` retains its earlier decode-pending stage.

Separate actual-window observation of a dedicated portable OBS also passes:
the Korean connection form, all five routing rows, the final include/exclude
preview, cancellation before apply, and main-screen Korean/English/Japanese
changes were observed. This is not an audit of every translated dialog state.

Full physical keyboard/mouse use, real new hardware/app capture creation/selection,
save-failure injection, Simple-output/replay-buffer recording, live streaming,
complete language flows, real UAC interaction, and final personal listening are
not covered by this fixture. Production installation and startup readback
separately **pass**: the common installer engine installed and verified five
files, installed hashes match the tested build, and the restarted OBS log
reports successful 1.3.0 load. Existing audio fields and output mode/track
assignments match their pre-install fingerprints. Production OBS visual
verification remains **UNVERIFIED**; no personal settings are included here.

The pinned build uses OBS Studio **33.0.1**, Qt **6.11.1** from the official OBS
dependencies, MSVC **19.51.36260**, Windows SDK headers/tools **10.0.26100.0**
(NuGet packages **10.0.26100.9457**), CMake **4.4.4**, and Ninja **1.13.2**.
Compiler/SDK caches and generated outputs are not part of the source package.
The environment scripts change only the invoking PowerShell process.

## Portable x64 build

Run from the source root in PowerShell. This public bootstrap route downloads
the pinned toolchain and prepares dependencies locally; it does not require
a system-wide PATH change or a Visual Studio installation for the tested path.

```powershell
python tools/prepare_portable_toolchain.py
python -m pip install --target tools/python cmake==4.4.4 ninja==1.13.2
. ./tools/Enter-TRUEITEMBuild.ps1
cmake -S . -B build_portable_multi -G "Ninja Multi-Config" `
  '-DCMAKE_VS_PLATFORM_NAME=x64' `
  '-DCMAKE_SYSTEM_VERSION=10.0.26100.0' `
  '-DCMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION=10.0.26100.0' `
  '-DENABLE_POST_BUILD_INSTALL=OFF' '-DENABLE_PREVIEW_TESTS=ON'
cmake --build build_portable_multi --config RelWithDebInfo --parallel 6
ctest --test-dir build_portable_multi -C RelWithDebInfo --output-on-failure
```

Quote each `-D...=...` argument, including dotted SDK versions, so PowerShell
passes it as one argument. Build output is not installed automatically.
With Visual Studio C++ tools and the Windows SDK installed, the original
`windows-x64` preset remains available; keep `ENABLE_POST_BUILD_INSTALL=OFF`.
Its generator must not share a build directory with the portable Ninja build.

The downloader uses the included public Visual Studio catalog to pin compiler
payloads and verifies their Microsoft catalog SHA256 values. Windows SDK
packages have pinned hashes. Tool licenses still apply; no compiler, SDK,
OBS, Qt, or PowerShell runtime is redistributed in the plugin ZIP.

## SDK bootstrap and optional cache reuse

The normal bootstrap downloads pinned OBS dependencies and builds `libobs` and
`obs-frontend-api` under `.deps`. It supplies Visual Studio architecture arguments
only to Visual Studio generators and uses Ninja Multi-Config for portable SDK
Debug/Release exports.

OBS 33.0.1's extracted build source receives an idempotent gate that skips
unneeded Win32 capture/injection helpers for SDK-only builds with plugins
disabled. The exact change is recorded in
[docs/portable-obs-sdk-patch.md](../docs/portable-obs-sdk-patch.md). The official
source ZIP is retained, and the installed OBS application is not modified.
The fallback architecture is restricted to a compiler-confirmed x64 target.

An optional public adapter at
[`build-aux/sdk-cache/use-prebuilt-sdk.cmake`](../build-aux/sdk-cache/use-prebuilt-sdk.cmake)
can reuse an already exported **matching OBS 33.0.1 SDK** under `.deps`. It is
selected explicitly with `CMAKE_PROJECT_TOP_LEVEL_INCLUDES` and its companion
[`cmake/buildspec.cmake`](../build-aux/sdk-cache/cmake/buildspec.cmake). It checks
the expected version and required SDK files, then skips SDK download/configure/
build. It does not create a missing cache or establish that an arbitrary cache
matches the source. Use this configure argument only after preparing the
matching exports:

```powershell
$sdkAdapter = Join-Path (Get-Location) 'build-aux/sdk-cache/use-prebuilt-sdk.cmake'
# Add this argument to the configure command above:
"-DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=$sdkAdapter"
```

The self-contained development build uses this cache-reuse route. Its public
adapter and build controls are included with the source; ignored local wrappers
are conveniences, not required distribution inputs. The cached binaries are
excluded. A clean environment should start with the public bootstrap route.

This is a pinned **Windows x64** build route. ARM64, every generator, all SDK
versions, and future OBS releases are not covered by the current evidence. If
the upstream architecture helper loses the expected anchor, the bootstrap stops
for inspection. Deprecated OBS flags and libebur128's older CMake compatibility
can emit warnings with the pinned versions.

## Graphical installer and its checks

The Windows entry is `CVA-VEIL-Setup.exe`, a console-free host for the same
PowerShell/WPF setup UI and transaction engine used by the script fallback.
Build the host from the public sources with:

```powershell
./build-aux/Build-SetupHost.ps1 -OutputDirectory ./work/installer-host-final
```

This uses the installed Windows .NET Framework C# compiler and Windows
PowerShell 5.1 `System.Management.Automation` assembly. The command above writes
the final host path expected by the packaging script. With no explicit
`-OutputDirectory`, the host builder instead defaults to
`work/installer-host/CVA-VEIL-Setup.exe`. The host does not require a separately
bundled PowerShell runtime.

The installer manages **five files**: the plugin DLL, three OBS locale files,
and `data/i18n.json`. It preserves the user's plugin configuration, checks that
OBS is closed, backs up replaced files, and restores them after a replacement
failure. The synthetic-file engine checks run with:

```powershell
./build-aux/tests/installer-core.tests.ps1
```

All **10 checks pass on Windows PowerShell 5.1 and PowerShell 7**. The native
host's Korean/English/Japanese language changes and the lock while OBS is open
have also been observed in the actual Windows UI. Those observations do not
replace final bootstrap-error handling, real UAC interaction, or a complete
GUI-launched install/launch end-to-end check. See
[`build-aux/INSTALLER.md`](../build-aux/INSTALLER.md) for the installer entry
points, preview mode, and transaction details.

## Output, packaging, and corresponding source

The DLL is produced at:

```text
build_portable_multi/RelWithDebInfo/loudness-balance-monitor.dll
```

The historical module name is retained for configuration and dock compatibility.
The visible product name is **CVA-VEIL-Auto Sound Setting**; archives use the
filename slug `CVA-VEIL-Auto-Sound-Setting` and version **1.3.0**.

After completing the matching build and runtime checks, inspect and create the
local binary/source pair with the public packaging entry:

```powershell
python build-aux/package.py --check
python build-aux/package.py
```

Defaults resolve the source root from the script location and write to `dist`.
Packaging uses existing build/installer outputs; it does not build, install,
start OBS, or upload anything. Keep generated tools, cached dependencies, private
OBS/BRIDGE settings, source gains, credentials, browser URLs, and test audio out
of distribution archives.

The original work is AllegroMoltoV's Loudness Balance Monitor v1.0.2, commit
`de4297086b28efdf5ac0990dd795cdb20b20d2c2`, licensed GPL-2.0. Modified source,
build controls, installer sources, and notices must correspond to the binary
being distributed. The linked libebur128 v1.2.6 source and MIT notice remain in
`third-party/libebur128`. See
[THIRD-PARTY-NOTICES.md](../THIRD-PARTY-NOTICES.md) and
[MODIFICATIONS.md](../MODIFICATIONS.md).

The old 1.2.0 B319 strict music-only recording-silence result remains historical
**raw FAIL / INCONCLUSIVE**, with its transition-buffer explanation unproven.
It is not substituted with an overall fixture PASS or used as proof of 1.3
channel setup. Current build/core results, native installer observations, actual
OBS/MKV contents, keyboard/screen-reader use, and listening have distinct scopes.
