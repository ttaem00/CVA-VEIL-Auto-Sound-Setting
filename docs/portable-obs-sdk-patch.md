# Portable OBS SDK build patch

Date: 2026-11-09. Upstream OBS Studio 33.0.1, downloaded from the official tag ZIP and verified against the plugin's buildspec hash. Original `.deps/33.0.1.zip` remains unchanged.

The plugin's `cmake/common/buildspec_common.cmake` applies this idempotent gate to the extracted dependency's `cmake/windows/architecture.cmake`, immediately before the first `if(OBS_PARENT_ARCHITECTURE STREQUAL CMAKE_VS_PLATFORM_NAME)`:

```cmake
# TRUEITEM_PORTABLE_SDK_ONLY 2026-11-09: no unused capture helpers for SDK builds.
if(DEFINED ENABLE_PLUGINS AND NOT ENABLE_PLUGINS)
  return()
endif()
```

Reason: the upstream architecture helper unconditionally configures a Win32 capture/injection helper tree using Visual Studio's `-A` even though the SDK-only parent passes `-DENABLE_PLUGINS=OFF`. Ninja cannot accept `-A`. Those helpers are unnecessary for `libobs` and `obs-frontend-api` targets, which are all the plugin bootstrap builds. The gate preserves architecture identification and the main dependency project still configures/builds its x64 libraries.

The installed OBS program, user settings, and official source archive are not changed. The modification is confined to the extracted build dependency and is reproducible from the plugin bootstrap. The plugin bootstrap also selects `Ninja Multi-Config` for its dependency build and only supplies `-A` to Visual Studio generators, preserving Debug/Release SDK exports. The Windows dependency architecture falls back to x64 only for a confirmed 64-bit compiler.
