// Stand-ins for the parts of the mhp3rd host that the Tekken 6 host does not
// have: mods, translation, fast loading, fast forward, overlays and the
// settings file. The shared kernel and firmware code (kernel/, hle/ under
// profiles/mhp3rd/host) calls these; each is the "feature is off" answer.
//
// When the Tekken host grows a real version of one of these, delete its shim
// here rather than editing the shared code.

#include "gpu/ge_state.hpp"
#include "kernel/fast_forward.hpp"
#include "kernel/fast_loading.hpp"
#include "mods/mhp3rd_mods.hpp"
#include "overlays.hpp"
#include "settings/settings.hpp"
#include "text/translation.hpp"

namespace mhp3rd {

// Tekken 6 has no code overlays of the kind Monster Hunter loads from DATA.BIN.
void revalidate_overlays(psprecomp::Runtime &) {}
void forget_unmatched_overlays() {}

namespace fast_forward {
double speed() { return 1.0; }
} // namespace fast_forward

namespace fast_loading {
void note_disc_read() {}
void update() {}
bool active() { return false; }
} // namespace fast_loading

namespace gpu {
void note_vram_copy(std::uint32_t, std::uint32_t, std::uint32_t) {}
} // namespace gpu

namespace mods {
void attach_disc(IsoImage *) {}
void code_loaded(psprecomp::Runtime &) {}
std::optional<DiscRange> data_bin_on_disc() { return std::nullopt; }
bool serving() { return false; }
std::uint64_t data_bin_size() { return 0u; }
std::size_t read_data_bin(std::uint64_t, std::span<std::uint8_t>) { return 0u; }
} // namespace mods

namespace text {
void translate_read(std::uint64_t, std::span<std::uint8_t>) {}
void note_loader_completion(const psprecomp::GuestMemory &, std::uint32_t, std::uint32_t) {}
} // namespace text

namespace settings {
Settings &current() {
    static Settings settings{};
    return settings;
}
} // namespace settings

} // namespace mhp3rd
