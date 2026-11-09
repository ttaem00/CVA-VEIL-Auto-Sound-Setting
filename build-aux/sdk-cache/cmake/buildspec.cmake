# Local prebuilt SDK cache adapter. This shadows buildspec only when explicitly
# selected by work/Build.ps1; no tracked CMake/source files are changed.
include_guard(GLOBAL)
set(_cva_deps "${CMAKE_CURRENT_SOURCE_DIR}/.deps")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/buildspec.json" _cva_spec)
string(JSON _cva_obs_version GET "${_cva_spec}" dependencies obs-studio version)
if(NOT _cva_obs_version STREQUAL "33.0.1")
  message(FATAL_ERROR "The local exported OBS SDK is 33.0.1. Prepare a matching cache before using a different OBS dependency.")
endif()
foreach(_cva_required
    "cmake/libobsConfig.cmake" "cmake/obs-frontend-apiConfig.cmake"
    "lib/obs.lib" "lib/obs-frontend-api.lib"
    "include/obs.h" "include/obs-frontend-api.h"
    "obs-deps-2026-10-23-x64/share/obs-deps/VERSION"
    "obs-deps-qt6-2026-10-23-x64/lib/cmake/Qt6/Qt6Config.cmake")
  if(NOT EXISTS "${_cva_deps}/${_cva_required}")
    message(FATAL_ERROR "Missing local SDK cache: ${_cva_required}")
  endif()
endforeach()
set(CMAKE_PREFIX_PATH
    "${_cva_deps}/obs-deps-2026-10-23-x64;${_cva_deps}/obs-deps-qt6-2026-10-23-x64;${_cva_deps}"
    CACHE PATH "D-drive local dependency caches" FORCE)
set(libobs_DIR "${_cva_deps}/cmake" CACHE PATH "Local exported libobs SDK" FORCE)
set(obs-frontend-api_DIR "${_cva_deps}/cmake" CACHE PATH "Local exported frontend SDK" FORCE)
set(Qt6_DIR "${_cva_deps}/obs-deps-qt6-2026-10-23-x64/lib/cmake/Qt6" CACHE PATH "Local Qt SDK" FORCE)
set(CVA_PREBUILT_SDK_CACHE_OVERRIDE TRUE CACHE INTERNAL "Local SDK export reused without rebuilding OBS")
message(STATUS "CVA local build: reusing D-drive OBS 33.0.1 SDK export; no SDK configure/build/download")
