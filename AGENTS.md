# VEIL Auto Sound Setting — Agent & Multi-LLM Operating Guide

This public repository (`ttaem00/CVA-VEIL-Auto-Sound-Setting`) contains **VEIL Auto Sound Setting**, a GPL-2.0 Windows x64 OBS Studio plugin (verified against OBS Studio 33.0.1) for separate live-stream vs local-recording audio track routing and 30-second loudness checks.

## 1. Core Invariants, Licensing & Safety

- Preserve GPL-2.0 compliance and upstream attribution ([LICENSE](LICENSE), [MODIFICATIONS.md](MODIFICATIONS.md), [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)). Corresponding source and SHA-256 parity must accompany binary releases.
- Never alter user audio levels or OBS track routing silently: preview/apply confirmation, exact undo, idle-encoder cleanup, and transactional installer backup/rollback (`build-aux/INSTALLER.md`) are mandatory invariants.
- Never commit local toolchain downloads (`tools/python`, `tools/portable-msvc`, `tools/sdk`, `.deps`, `build_portable_multi/`), personal OBS configs, machine-local paths, or credentials.

## 2. Build & Validation Commands

Follow the portable x64 build in [tools/README.md](tools/README.md) from PowerShell 7 (in a fresh clone/worktree, bootstrap once via `python tools/prepare_portable_toolchain.py` and `python -m pip install --target tools/python cmake==4.4.4 ninja==1.13.2`):

```powershell
. ./tools/Enter-TRUEITEMBuild.ps1
cmake -S . -B build_portable_multi -G "Ninja Multi-Config" '-DCMAKE_VS_PLATFORM_NAME=x64' '-DCMAKE_SYSTEM_VERSION=10.0.26100.0' '-DCMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION=10.0.26100.0' '-DENABLE_POST_BUILD_INSTALL=OFF' '-DENABLE_PREVIEW_TESTS=ON'
cmake --build build_portable_multi --config RelWithDebInfo --parallel 6
ctest --test-dir build_portable_multi -C RelWithDebInfo --output-on-failure
```

- Keep CTest unit checks (`5/5` core tests), encoded MKV fixture checks, installer readback, and live OBS observation as distinct evidence scopes.

## 3. Multi-LLM Coexistence (`Codex` · `Claude Code` · `Antigravity`)

- `AGENTS.md` is the single canonical instruction file across all coding agents; do not create duplicate root instruction files.
- Preserve existing checkout state; work in an isolated issue branch/worktree (`codex/issue-<N>-<slug>`) and merge via **merge-commit PRs only** (no direct `main` push, no squash/rebase, no `--no-verify`) after independent `/adversarial-review`.
