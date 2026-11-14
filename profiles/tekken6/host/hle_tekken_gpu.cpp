// A headless display and GE layer for the Tekken 6 bring-up host.
//
// There is no renderer here. The calls answer as a PSP whose GPU finishes every
// list instantly: the video memory base and size are the real ones (the game
// computes its frame buffer addresses from them), lists get ids, and syncs report
// done. Frame buffer flips and display modes are accepted and ignored. A real
// renderer replaces this file's graphics half; the vblank waits stay.

#include "hle_tekken.hpp"

#include "kernel/kernel.hpp"

#include <cstdint>

namespace tekken6 {
namespace {

using mhp3rd::arg;
using mhp3rd::kernel;
using mhp3rd::WaitState;
using mhp3rd::WaitType;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;

struct NidEntry {
    const char *library;
    const char *name;
    std::uint32_t nid;
};

constexpr NidEntry kNids[] = {
    {"sceDisplay", "sceDisplaySetMode", 0x0E20F177u},
    {"sceDisplay", "sceDisplaySetFrameBuf", 0x289D82FEu},
    {"sceDisplay", "sceDisplayGetFrameBuf", 0xEEDA2E54u},
    {"sceDisplay", "sceDisplayWaitVblankStart", 0x984C27E7u},
    {"sceDisplay", "sceDisplayWaitVblankStartCB", 0x46F186C3u},
    {"sceDisplay", "sceDisplayWaitVblank", 0x36CDFADEu},
    {"sceDisplay", "sceDisplayWaitVblankCB", 0x8EB9EC49u},
    {"sceDisplay", "sceDisplayGetVcount", 0x9C6EAAD7u},
    {"sceDisplay", "sceDisplayIsVblank", 0x4D4E10ECu},
    {"sceGe_user", "sceGeEdramGetAddr", 0xE47E40E4u},
    {"sceGe_user", "sceGeEdramGetSize", 0x1F6752ADu},
    {"sceGe_user", "sceGeEdramSetAddrTranslation", 0xB77905EAu},
    {"sceGe_user", "sceGeSetCallback", 0xA4FC06A4u},
    {"sceGe_user", "sceGeUnsetCallback", 0x05DB22CEu},
    {"sceGe_user", "sceGeListEnQueue", 0xAB49E76Au},
    {"sceGe_user", "sceGeListEnQueueHead", 0x1C0D95A6u},
    {"sceGe_user", "sceGeListUpdateStallAddr", 0xE0D68148u},
    {"sceGe_user", "sceGeListSync", 0x03444EB4u},
    {"sceGe_user", "sceGeDrawSync", 0xB287BD61u},
    {"sceGe_user", "sceGeBreak", 0xB448EC0Du},
    {"sceGe_user", "sceGeContinue", 0x4C06E472u},
};

constexpr std::uint32_t kEdramBase = 0x04000000u;
constexpr std::uint32_t kEdramSize = 0x00200000u;

void finish_zero(Runtime &, AllegrexContext &ctx) { kernel().finish(ctx, 0u); }

// The thread sleeps until the next vblank, then returns 0.
void wait_vblank(Runtime &, AllegrexContext &ctx) {
    WaitState wait{};
    wait.type = WaitType::VBlank;
    kernel().block(ctx, wait, 0u);
}

std::uint32_t g_next_list_id = 1u;
std::uint32_t g_next_callback_id = 1u;

} // namespace

void add_tekken_gpu_nids(Runtime &runtime) {
    for (const NidEntry &entry : kNids) runtime.nids().add(entry.library, entry.nid, entry.name);
}

void register_tekken_gpu(mhp3rd::HleRegistrar &hle) {
    hle.add("sceDisplay", "sceDisplaySetMode", finish_zero);
    hle.add("sceDisplay", "sceDisplaySetFrameBuf", finish_zero);
    hle.add("sceDisplay", "sceDisplayGetFrameBuf", finish_zero);
    hle.add("sceDisplay", "sceDisplayIsVblank", finish_zero);
    hle.add("sceDisplay", "sceDisplayGetVcount", [](Runtime &, AllegrexContext &ctx) {
        kernel().finish(ctx, static_cast<std::uint32_t>(kernel().vblank_count()));
    });
    hle.add("sceDisplay", "sceDisplayWaitVblankStart", wait_vblank);
    hle.add("sceDisplay", "sceDisplayWaitVblankStartCB", wait_vblank);
    hle.add("sceDisplay", "sceDisplayWaitVblank", wait_vblank);
    hle.add("sceDisplay", "sceDisplayWaitVblankCB", wait_vblank);

    hle.add("sceGe_user", "sceGeEdramGetAddr", [](Runtime &, AllegrexContext &ctx) { kernel().finish(ctx, kEdramBase); });
    hle.add("sceGe_user", "sceGeEdramGetSize", [](Runtime &, AllegrexContext &ctx) { kernel().finish(ctx, kEdramSize); });
    hle.add("sceGe_user", "sceGeEdramSetAddrTranslation", [](Runtime &, AllegrexContext &ctx) { kernel().finish(ctx, kEdramSize); });
    hle.add("sceGe_user", "sceGeSetCallback", [](Runtime &, AllegrexContext &ctx) {
        kernel().finish(ctx, g_next_callback_id++);
    });
    hle.add("sceGe_user", "sceGeUnsetCallback", finish_zero);
    hle.add("sceGe_user", "sceGeListEnQueue", [](Runtime &, AllegrexContext &ctx) { kernel().finish(ctx, g_next_list_id++); });
    hle.add("sceGe_user", "sceGeListEnQueueHead", [](Runtime &, AllegrexContext &ctx) { kernel().finish(ctx, g_next_list_id++); });
    hle.add("sceGe_user", "sceGeListUpdateStallAddr", finish_zero);
    hle.add("sceGe_user", "sceGeListSync", finish_zero);  // 0: the list is done
    hle.add("sceGe_user", "sceGeDrawSync", finish_zero);  // 0: drawing is done
    hle.add("sceGe_user", "sceGeBreak", finish_zero);
    hle.add("sceGe_user", "sceGeContinue", finish_zero);
}

} // namespace tekken6
