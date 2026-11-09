// Test-only OBS 33 API surface, 2026-11-09. SPDX-License-Identifier: GPL-2.0
#pragma once
#include <media-io/audio-io.h>
struct obs_source;
using obs_source_t = obs_source;
#define OBS_SOURCE_AUDIO 2
using capture_callback_t = void (*)(void *, obs_source_t *, const audio_data *, bool);
audio_t *obs_get_audio();
obs_source_t *obs_get_source_by_name(const char *);
uint32_t obs_source_get_output_flags(const obs_source_t *);
void obs_source_release(obs_source_t *);
void obs_source_add_audio_capture_callback(obs_source_t *, capture_callback_t, void *);
void obs_source_remove_audio_capture_callback(obs_source_t *, capture_callback_t, void *);
float obs_source_get_volume(const obs_source_t *);
bool obs_source_muted(const obs_source_t *);
