// Test-only OBS 33 API surface, 2026-11-09. SPDX-License-Identifier: GPL-2.0
#pragma once
#include <cstddef>
#include <cstdint>
#define MAX_AUDIO_MIXES 6
#define MAX_AV_PLANES 8
enum audio_format { AUDIO_FORMAT_FLOAT_PLANAR = 8 };
enum speaker_layout { SPEAKERS_MONO = 1, SPEAKERS_STEREO = 2 };
struct audio_data { uint8_t *data[MAX_AV_PLANES]{}; uint32_t frames{}; uint64_t timestamp{}; };
struct audio_convert_info { uint32_t samples_per_sec{}; audio_format format{}; speaker_layout speakers{}; bool allow_clipping{}; };
struct audio_output;
using audio_t = audio_output;
using audio_output_callback_t = void (*)(void *, size_t, audio_data *);
bool audio_output_connect(audio_t *, size_t, const audio_convert_info *, audio_output_callback_t, void *);
void audio_output_disconnect(audio_t *, size_t, audio_output_callback_t, void *);
uint32_t audio_output_get_sample_rate(const audio_t *);
size_t audio_output_get_channels(const audio_t *);
