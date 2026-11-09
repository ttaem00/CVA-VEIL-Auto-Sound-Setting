// CVA VEIL modification, 2026-11-09. SPDX-License-Identifier: GPL-2.0
#pragma once
#include <cstdint>

namespace lbm {
struct RouteChoice { uint32_t before{}; bool stream{}; bool record{}; };
// Inclusion adds the main mix while preserving dedicated editing assignments.
// Exclusion removes EVERY selected recording track. Bits outside these outputs
// remain intact; a stream/record overlap is resolved before using this function.
inline uint32_t routedMask(RouteChoice choice, uint32_t stream, uint32_t recording, uint32_t main)
{
    uint32_t mask = choice.before;
    mask = choice.stream ? mask | stream : mask & ~stream;
    mask = choice.record ? mask | main : mask & ~recording;
    return mask;
}
inline uint32_t firstTrack(uint32_t mask) { return mask & (~mask + 1u); }
inline uint32_t distinctRecordingMask(uint32_t existing, uint32_t stream, uint32_t vod)
{
    uint32_t mask = (existing & 63u) & ~stream & ~vod;
    if (mask) return mask;
    // Do not reuse the service's archive output for a new recording mix.
    return firstTrack(63u & ~stream & ~vod);
}
} // namespace lbm
