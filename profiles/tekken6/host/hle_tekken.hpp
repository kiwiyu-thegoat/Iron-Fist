#pragma once

#include "hle/hle_common.hpp"

namespace tekken6 {

// Teaches the runtime the names of the firmware calls Tekken 6 imports that the
// shared NID table does not list. The names are fixed by their NIDs (a NID is
// the first four bytes of the SHA-1 of the function's name), so each entry is
// checkable. Call before constructing the HleRegistrar.
void add_tekken_nids(psprecomp::Runtime &runtime);

// The firmware calls Tekken 6 makes that the shared mhp3rd host does not
// implement: thread and timer objects, lightweight mutexes, the clock, power
// and system information, and network state (always "not connected").
void register_tekken_firmware(mhp3rd::HleRegistrar &hle);

// The clock (sceRtc), clock speeds, language, UMD and network state.
void register_tekken_system(mhp3rd::HleRegistrar &hle);

// Headless display and GE: correct video memory addresses, vblank waits, and
// lists that finish instantly. No drawing.
void add_tekken_gpu_nids(psprecomp::Runtime &runtime);
void register_tekken_gpu(mhp3rd::HleRegistrar &hle);

} // namespace tekken6
