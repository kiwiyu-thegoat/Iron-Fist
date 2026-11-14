#include "hle_tekken.hpp"

#include "kernel/kernel.hpp"

#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace tekken6 {
namespace {

using mhp3rd::arg;
using mhp3rd::arg64;
using mhp3rd::kernel;
using mhp3rd::Mutex;
using mhp3rd::SceUID;
using mhp3rd::WaitState;
using mhp3rd::WaitType;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
namespace error = mhp3rd::error;

struct NidEntry {
    const char *library;
    const char *name;
    std::uint32_t nid;
};

constexpr NidEntry kNids[] = {
    {"ThreadManForUser", "sceKernelTryLockMutex", 0x0DDCD2C9u},
    {"ThreadManForUser", "sceKernelWaitThreadEndCB", 0x840E8133u},
    {"ThreadManForUser", "sceKernelCreateLwMutex", 0x19CFF145u},
    {"ThreadManForUser", "sceKernelCreateFpl", 0xC07BB470u},
    {"ThreadManForUser", "sceKernelDeleteFpl", 0xED1410E0u},
    {"ThreadManForUser", "sceKernelTryAllocateFpl", 0x623AE665u},
    {"ThreadManForUser", "sceKernelFreeFpl", 0xF6414A71u},
    {"ThreadManForUser", "sceKernelStopVTimer", 0xD0AEEE87u},
    {"ThreadManForUser", "sceKernelDeleteVTimer", 0x328F9E52u},
    {"ThreadManForUser", "sceKernelGetVTimerTime", 0x034A921Fu},
    {"ThreadManForUser", "sceKernelSetVTimerTime", 0x542AD630u},
    {"ThreadManForUser", "sceKernelCheckThreadStack", 0xD13BDE95u},
    {"ThreadManForUser", "sceKernelGetThreadStackFreeSize", 0x52089CA1u},
    {"ThreadManForUser", "sceKernelCancelWakeupThread", 0xFCCFAD26u},
    {"ThreadManForUser", "sceKernelCheckCallback", 0x349D6D6Cu},
    {"ThreadManForUser", "sceKernelWaitThreadEnd", 0x278C0DF5u},
    {"ThreadManForUser", "sceKernelSysClock2USec", 0xBA6B92E2u},
    {"Kernel_Library", "sceKernelLockLwMutex", 0xBEA46419u},
    {"Kernel_Library", "sceKernelUnlockLwMutex", 0x15B6446Bu},
    {"Kernel_Library", "sceKernelCpuResumeIntrWithSync", 0x3B84732Du},
    {"ModuleMgrForUser", "sceKernelLoadModule", 0x977DE386u},
    {"sceRtc", "sceRtcGetCurrentTick", 0x3F7AD767u},
    {"sceRtc", "sceRtcGetCurrentClock", 0x4CFA57B0u},
    {"sceRtc", "sceRtcGetDayOfWeek", 0x57726BC1u},
    {"sceRtc", "sceRtcGetDaysInMonth", 0x05EF322Cu},
    {"sceRtc", "sceRtcGetTick", 0x6FF40ACCu},
    {"sceRtc", "sceRtcGetTime_t", 0x27C4594Cu},
    {"sceRtc", "sceRtcSetTick", 0x7ED29E40u},
    {"sceRtc", "sceRtcTickAddDays", 0xE51B4B7Au},
    {"sceRtc", "sceRtcTickAddHours", 0x26D7A24Au},
    {"sceRtc", "sceRtcTickAddMinutes", 0xE6605BCAu},
    {"sceRtc", "sceRtcTickAddSeconds", 0xF2A4AFE5u},
    {"scePower", "scePowerGetBusClockFrequencyInt", 0xBD681969u},
    {"scePower", "scePowerGetCpuClockFrequencyInt", 0xFDB5BFE9u},
    {"scePower", "scePowerGetPllClockFrequencyInt", 0x34F9C463u},
    {"sceImpose", "sceImposeGetLanguageMode", 0x24FD7BCFu},
    {"sceCtrl", "sceCtrlSetIdleCancelThreshold", 0xA7144800u},
    {"sceUmdUser", "sceUmdDeactivate", 0xE83742BAu},
    {"sceUmdUser", "sceUmdWaitDriveStatCB", 0x4A9E5E29u},
    {"sceUtility", "sceUtilityGetSystemParamString", 0x34B78343u},
    {"sceUtility", "sceUtilityMsgDialogAbort", 0x4928BD96u},
    {"sceNetAdhocctl", "sceNetAdhocctlConnect", 0x0AD043EDu},
    {"sceNetAdhocctl", "sceNetAdhocctlGetState", 0x75ECD386u},
    {"sceNetApctl", "sceNetApctlGetState", 0x5DEAC81Bu},
    {"sceNetInet", "sceNetInetInetAton", 0x1BDF5D13u},};

SceUID as_signed(std::uint32_t value) { return static_cast<SceUID>(value); }

std::uint64_t load64(const psprecomp::GuestMemory &memory, std::uint32_t address) {
    return static_cast<std::uint64_t>(memory.load32(address)) |
        (static_cast<std::uint64_t>(memory.load32(address + 4u)) << 32u);
}

// ---- mutexes ---------------------------------------------------------------

constexpr std::uint32_t kMutexAttrRecursive = 0x200u;

void lock_mutex(AllegrexContext &ctx, SceUID uid, std::int32_t count, std::uint32_t timeout_address, bool block) {
    auto found = kernel().mutexes.find(uid);
    if (found == kernel().mutexes.end()) {
        kernel().finish(ctx, error::kMutexNotFound);
        return;
    }
    if (count <= 0) {
        kernel().finish(ctx, error::kIllegalCount);
        return;
    }
    Mutex &mutex = found->second;
    const SceUID me = kernel().current_uid();
    if (mutex.lock_count == 0) {
        mutex.owner = me;
        mutex.lock_count = count;
        kernel().finish(ctx, 0u);
        return;
    }
    if (mutex.owner == me) {
        if ((mutex.attributes & kMutexAttrRecursive) == 0u) {
            kernel().finish(ctx, error::kMutexRecursiveNotAllowed);
            return;
        }
        mutex.lock_count += count;
        kernel().finish(ctx, 0u);
        return;
    }
    if (!block) {
        kernel().finish(ctx, error::kMutexLocked);
        return;
    }
    mutex.waiters.push_back(me);
    WaitState wait{};
    wait.type = WaitType::Mutex;
    wait.object = uid;
    wait.value = static_cast<std::uint32_t>(count);
    wait.timeout_address = timeout_address;
    kernel().block(ctx, wait, 0u);
}

void unlock_mutex(AllegrexContext &ctx, SceUID uid, std::int32_t count) {
    auto found = kernel().mutexes.find(uid);
    if (found == kernel().mutexes.end()) {
        kernel().finish(ctx, error::kMutexNotFound);
        return;
    }
    Mutex &mutex = found->second;
    if (count <= 0)
        kernel().finish(ctx, error::kIllegalCount);
    else if (mutex.lock_count == 0 || mutex.owner != kernel().current_uid())
        kernel().finish(ctx, error::kMutexUnlocked);
    else if (count > mutex.lock_count)
        kernel().finish(ctx, error::kMutexUnlockUnderflow);
    else {
        mutex.lock_count -= count;
        if (mutex.lock_count == 0) {
            mutex.owner = 0;
            kernel().release_mutex_waiters(uid);
        }
        kernel().finish(ctx, 0u);
    }
}

// A lightweight mutex is a kernel mutex whose uid the game keeps in its own
// workarea (at +16, as SceLwMutexWorkarea lays it out) and passes to every call.
constexpr std::uint32_t kLwMutexUidOffset = 16u;

SceUID lw_uid(const Runtime &rt, std::uint32_t workarea) {
    return as_signed(rt.memory().load32(workarea + kLwMutexUidOffset));
}

void register_mutexes(mhp3rd::HleRegistrar &hle) {
    hle.add("ThreadManForUser", "sceKernelTryLockMutex", [](Runtime &, AllegrexContext &ctx) {
        lock_mutex(ctx, as_signed(arg(ctx, 0)), as_signed(arg(ctx, 1)), 0u, false);
    });
    hle.add("ThreadManForUser", "sceKernelCreateLwMutex", [](Runtime &rt, AllegrexContext &ctx) {
        const std::uint32_t workarea = arg(ctx, 0);
        const SceUID uid = kernel().allocate_uid();
        Mutex mutex{mhp3rd::read_cstring(rt.memory(), arg(ctx, 1), 32u), arg(ctx, 2), 0, as_signed(arg(ctx, 3)), {}};
        if (mutex.lock_count > 0) mutex.owner = kernel().current_uid();
        kernel().mutexes[uid] = std::move(mutex);
        rt.memory().store32(workarea + 0u, arg(ctx, 3));
        rt.memory().store32(workarea + 8u, arg(ctx, 2));
        rt.memory().store32(workarea + kLwMutexUidOffset, static_cast<std::uint32_t>(uid));
        kernel().finish(ctx, 0u);
    });
    hle.add("Kernel_Library", "sceKernelLockLwMutex", [](Runtime &rt, AllegrexContext &ctx) {
        lock_mutex(ctx, lw_uid(rt, arg(ctx, 0)), as_signed(arg(ctx, 1)), arg(ctx, 2), true);
    });
    hle.add("Kernel_Library", "sceKernelUnlockLwMutex", [](Runtime &rt, AllegrexContext &ctx) {
        unlock_mutex(ctx, lw_uid(rt, arg(ctx, 0)), as_signed(arg(ctx, 1)));
    });
}

// ---- threads ---------------------------------------------------------------

void wait_thread_end(AllegrexContext &ctx) {
    const SceUID uid = as_signed(arg(ctx, 0));
    mhp3rd::Thread *thread = kernel().find_thread(uid);
    if (thread == nullptr) {
        kernel().finish(ctx, error::kUnknownThid);
        return;
    }
    if (thread->status == mhp3rd::ThreadStatus::Dormant) {
        kernel().finish(ctx, static_cast<std::uint32_t>(thread->exit_status));
        return;
    }
    WaitState wait{};
    wait.type = WaitType::ThreadEnd;
    wait.object = uid;
    wait.timeout_address = arg(ctx, 1);
    kernel().block(ctx, wait, 0u);
}

void register_threads(mhp3rd::HleRegistrar &hle) {
    hle.add("ThreadManForUser", "sceKernelWaitThreadEnd", [](Runtime &, AllegrexContext &ctx) { wait_thread_end(ctx); });
    hle.add("ThreadManForUser", "sceKernelWaitThreadEndCB", [](Runtime &, AllegrexContext &ctx) {
        (void)kernel().deliver_callbacks();
        wait_thread_end(ctx);
    });
    // The host gives threads generous stacks and does not track the low-water mark.
    hle.add("ThreadManForUser", "sceKernelCheckThreadStack",
            [](Runtime &, AllegrexContext &ctx) { kernel().finish(ctx, 0x2000u); });
    hle.add("ThreadManForUser", "sceKernelGetThreadStackFreeSize",
            [](Runtime &, AllegrexContext &ctx) { kernel().finish(ctx, 0x2000u); });
    // Wakeup requests are not counted, so there is nothing to cancel.
    hle.add("ThreadManForUser", "sceKernelCancelWakeupThread",
            [](Runtime &, AllegrexContext &ctx) { kernel().finish(ctx, 0u); });
    hle.add("ThreadManForUser", "sceKernelCheckCallback", [](Runtime &, AllegrexContext &ctx) {
        (void)kernel().deliver_callbacks();
        kernel().finish(ctx, 0u);
    });
    hle.add("Kernel_Library", "sceKernelCpuResumeIntrWithSync", [](Runtime &, AllegrexContext &ctx) {
        kernel().set_interrupts_enabled(arg(ctx, 0) != 0u);
        kernel().finish(ctx, 0u);
    });
    hle.add("ModuleMgrForUser", "sceKernelLoadModule", [](Runtime &, AllegrexContext &ctx) {
        // The firmware libraries a title loads (PSMF and so on) are provided by
        // the host, so loading one only has to hand back an id for StartModule.
        kernel().finish(ctx, static_cast<std::uint32_t>(kernel().allocate_uid()));
    });
}

// ---- virtual timers --------------------------------------------------------

void register_vtimers(mhp3rd::HleRegistrar &hle) {
    hle.add("ThreadManForUser", "sceKernelStopVTimer", [](Runtime &, AllegrexContext &ctx) {
        auto found = kernel().vtimers.find(as_signed(arg(ctx, 0)));
        if (found == kernel().vtimers.end()) {
            kernel().finish(ctx, error::kUnknownVtid);
            return;
        }
        mhp3rd::VTimer &timer = found->second;
        const bool was_active = timer.active;
        if (was_active) {
            timer.accumulated_us += kernel().now_us() - timer.base_us;
            timer.active = false;
        }
        kernel().finish(ctx, was_active ? 1u : 0u);
    });
    hle.add("ThreadManForUser", "sceKernelDeleteVTimer", [](Runtime &, AllegrexContext &ctx) {
        kernel().finish(ctx, kernel().vtimers.erase(as_signed(arg(ctx, 0))) != 0u ? 0u : error::kUnknownVtid);
    });
    hle.add("ThreadManForUser", "sceKernelGetVTimerTime", [](Runtime &rt, AllegrexContext &ctx) {
        auto found = kernel().vtimers.find(as_signed(arg(ctx, 0)));
        if (found == kernel().vtimers.end()) {
            kernel().finish(ctx, error::kUnknownVtid);
            return;
        }
        if (arg(ctx, 1) != 0u) mhp3rd::store64(rt.memory(), arg(ctx, 1), kernel().vtimer_value(found->second));
        kernel().finish(ctx, 0u);
    });
    hle.add("ThreadManForUser", "sceKernelSetVTimerTime", [](Runtime &rt, AllegrexContext &ctx) {
        auto found = kernel().vtimers.find(as_signed(arg(ctx, 0)));
        if (found == kernel().vtimers.end()) {
            kernel().finish(ctx, error::kUnknownVtid);
            return;
        }
        mhp3rd::VTimer &timer = found->second;
        const std::uint64_t previous = kernel().vtimer_value(timer);
        timer.accumulated_us = arg(ctx, 1) != 0u ? load64(rt.memory(), arg(ctx, 1)) : 0u;
        timer.base_us = kernel().now_us();
        kernel().finish(ctx, static_cast<std::uint32_t>(previous));
    });
}

// ---- fixed-size pools ------------------------------------------------------

struct Fpl {
    std::uint32_t base{};
    std::uint32_t block_size{};
    SceUID block{};
    std::vector<bool> used;
};
std::map<SceUID, Fpl> g_fpls;

void register_fixed_pools(mhp3rd::HleRegistrar &hle) {
    hle.add("ThreadManForUser", "sceKernelCreateFpl", [](Runtime &rt, AllegrexContext &ctx) {
        // (name, partition, attributes, block size, block count, option)
        const std::uint32_t block_size = (arg(ctx, 3) + 63u) & ~63u;
        const std::uint32_t count = arg(ctx, 4);
        if (block_size == 0u || count == 0u) {
            kernel().finish(ctx, error::kIllegalArgument);
            return;
        }
        const SceUID block = kernel().allocate_block(
            mhp3rd::read_cstring(rt.memory(), arg(ctx, 0), 32u), 3u, block_size * count, 64u);
        const mhp3rd::MemoryBlock *memory = block >= 0 ? kernel().find_block(block) : nullptr;
        if (memory == nullptr) {
            kernel().finish(ctx, error::kNoMemory);
            return;
        }
        const SceUID uid = kernel().allocate_uid();
        g_fpls[uid] = Fpl{memory->address, block_size, block, std::vector<bool>(count, false)};
        kernel().finish(ctx, static_cast<std::uint32_t>(uid));
    });
    hle.add("ThreadManForUser", "sceKernelDeleteFpl", [](Runtime &, AllegrexContext &ctx) {
        auto found = g_fpls.find(as_signed(arg(ctx, 0)));
        if (found == g_fpls.end()) {
            kernel().finish(ctx, error::kUnknownUid);
            return;
        }
        (void)kernel().free_block(found->second.block);
        g_fpls.erase(found);
        kernel().finish(ctx, 0u);
    });
    hle.add("ThreadManForUser", "sceKernelTryAllocateFpl", [](Runtime &rt, AllegrexContext &ctx) {
        auto found = g_fpls.find(as_signed(arg(ctx, 0)));
        if (found == g_fpls.end()) {
            kernel().finish(ctx, error::kUnknownUid);
            return;
        }
        Fpl &pool = found->second;
        for (std::uint32_t i = 0; i < pool.used.size(); ++i) {
            if (pool.used[i]) continue;
            pool.used[i] = true;
            if (arg(ctx, 1) != 0u) rt.memory().store32(arg(ctx, 1), pool.base + i * pool.block_size);
            kernel().finish(ctx, 0u);
            return;
        }
        kernel().finish(ctx, error::kNoMemory); // every block is in use
    });
    hle.add("ThreadManForUser", "sceKernelFreeFpl", [](Runtime &, AllegrexContext &ctx) {
        auto found = g_fpls.find(as_signed(arg(ctx, 0)));
        if (found == g_fpls.end()) {
            kernel().finish(ctx, error::kUnknownUid);
            return;
        }
        Fpl &pool = found->second;
        const std::uint32_t address = arg(ctx, 1);
        const bool aligned = address >= pool.base && (address - pool.base) % pool.block_size == 0u;
        const std::uint32_t index = aligned ? (address - pool.base) / pool.block_size : 0u;
        if (!aligned || index >= pool.used.size() || !pool.used[index]) {
            kernel().finish(ctx, error::kIllegalMemblock);
            return;
        }
        pool.used[index] = false;
        kernel().finish(ctx, 0u);
    });
}

} // namespace

void add_tekken_nids(Runtime &runtime) {
    for (const NidEntry &entry : kNids) runtime.nids().add(entry.library, entry.nid, entry.name);
}

void register_tekken_firmware(mhp3rd::HleRegistrar &hle) {
    register_mutexes(hle);
    register_threads(hle);
    register_vtimers(hle);
    register_fixed_pools(hle);
}

} // namespace tekken6
