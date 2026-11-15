// Clock, power, language, UMD and network state for Tekken 6.
//
// Policy: the machine answers as a plain retail PSP with English and
// cross-to-confirm, and the network always reports "not connected", which every
// title has to handle.

#include "hle_tekken.hpp"

#include "kernel/kernel.hpp"

#include <chrono>
#include <cstdint>
#include <cstring>

namespace tekken6 {
namespace {

using mhp3rd::arg;
using mhp3rd::arg64;
using mhp3rd::kernel;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;

// ---- sceRtc ----------------------------------------------------------------
//
// A tick is a microsecond count since 0001-01-01 00:00:00. The calendar maths
// is Howard Hinnant's civil-date algorithms: no host time zone, no 32-bit
// time_t, and exact over the whole range a tick covers.

constexpr std::uint64_t kUnixEpochTicks = 62135596800000000ull;

std::uint64_t now_ticks() {
    using namespace std::chrono;
    const auto since_epoch = duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();
    return kUnixEpochTicks + static_cast<std::uint64_t>(since_epoch);
}

std::int64_t days_from_civil(std::int64_t y, unsigned m, unsigned d) {
    y -= m <= 2u;
    const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153u * (m + (m > 2u ? -3 : 9)) + 2u) / 5u + d - 1u;
    const unsigned doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;
    return era * 146097 + static_cast<std::int64_t>(doe) - 719468;
}

struct Civil {
    std::uint16_t year, month, day, hour, minute, second;
    std::uint32_t microsecond;
};

Civil civil_from_ticks(std::uint64_t ticks) {
    const std::uint64_t unix_us = ticks - kUnixEpochTicks;
    const std::int64_t secs = static_cast<std::int64_t>(unix_us / 1000000ull);
    const std::int64_t days = secs / 86400;
    const std::int64_t rem = secs % 86400;
    const std::int64_t z = days + 719468;
    const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
    const std::int64_t y = static_cast<std::int64_t>(yoe) + era * 400;
    const unsigned doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
    const unsigned mp = (5u * doy + 2u) / 153u;
    const unsigned d = doy - (153u * mp + 2u) / 5u + 1u;
    const unsigned m = mp < 10u ? mp + 3u : mp - 9u;
    return Civil{static_cast<std::uint16_t>(y + (m <= 2u ? 1 : 0)), static_cast<std::uint16_t>(m),
                 static_cast<std::uint16_t>(d), static_cast<std::uint16_t>(rem / 3600),
                 static_cast<std::uint16_t>(rem % 3600 / 60), static_cast<std::uint16_t>(rem % 60),
                 static_cast<std::uint32_t>(unix_us % 1000000ull)};
}

std::uint64_t ticks_from_civil(const Civil &c) {
    const std::int64_t days = days_from_civil(c.year, c.month, c.day);
    const std::int64_t secs = days * 86400 + c.hour * 3600 + c.minute * 60 + c.second;
    return kUnixEpochTicks + static_cast<std::uint64_t>(secs) * 1000000ull + c.microsecond;
}

// pspTime: six u16 fields, then a u32 of microseconds.
void store_psp_time(psprecomp::GuestMemory &memory, std::uint32_t address, const Civil &c) {
    memory.store16(address + 0u, c.year);
    memory.store16(address + 2u, c.month);
    memory.store16(address + 4u, c.day);
    memory.store16(address + 6u, c.hour);
    memory.store16(address + 8u, c.minute);
    memory.store16(address + 10u, c.second);
    memory.store32(address + 12u, c.microsecond);
}

Civil load_psp_time(const psprecomp::GuestMemory &memory, std::uint32_t address) {
    return Civil{memory.load16(address + 0u), memory.load16(address + 2u), memory.load16(address + 4u),
                 memory.load16(address + 6u), memory.load16(address + 8u), memory.load16(address + 10u),
                 memory.load32(address + 12u)};
}

std::uint64_t load_u64(const psprecomp::GuestMemory &memory, std::uint32_t address) {
    return static_cast<std::uint64_t>(memory.load32(address)) |
        (static_cast<std::uint64_t>(memory.load32(address + 4u)) << 32u);
}

// (u64 *destination, const u64 *source, s64 amount in a2:a3)
void tick_add(Runtime &rt, AllegrexContext &ctx, std::uint64_t unit_us) {
    const auto amount = static_cast<std::int64_t>(arg64(ctx, 2));
    const std::uint64_t base = load_u64(rt.memory(), arg(ctx, 1));
    mhp3rd::store64(rt.memory(), arg(ctx, 0), base + static_cast<std::uint64_t>(amount * static_cast<std::int64_t>(unit_us)));
    kernel().finish(ctx, 0u);
}

bool is_leap(unsigned year) { return (year % 4u == 0u && year % 100u != 0u) || year % 400u == 0u; }

void register_rtc(mhp3rd::HleRegistrar &hle) {
    hle.add("sceRtc", "sceRtcGetCurrentTick", [](Runtime &rt, AllegrexContext &ctx) {
        mhp3rd::store64(rt.memory(), arg(ctx, 0), now_ticks());
        kernel().finish(ctx, 0u);
    });
    hle.add("sceRtc", "sceRtcGetCurrentClock", [](Runtime &rt, AllegrexContext &ctx) {
        // (pspTime *, int time zone offset in minutes)
        const auto zone_us = static_cast<std::int64_t>(static_cast<std::int32_t>(arg(ctx, 1))) * 60 * 1000000;
        store_psp_time(rt.memory(), arg(ctx, 0),
                       civil_from_ticks(static_cast<std::uint64_t>(static_cast<std::int64_t>(now_ticks()) + zone_us)));
        kernel().finish(ctx, 0u);
    });
    hle.add("sceRtc", "sceRtcSetTick", [](Runtime &rt, AllegrexContext &ctx) {
        store_psp_time(rt.memory(), arg(ctx, 0), civil_from_ticks(load_u64(rt.memory(), arg(ctx, 1))));
        kernel().finish(ctx, 0u);
    });
    hle.add("sceRtc", "sceRtcGetTick", [](Runtime &rt, AllegrexContext &ctx) {
        mhp3rd::store64(rt.memory(), arg(ctx, 1), ticks_from_civil(load_psp_time(rt.memory(), arg(ctx, 0))));
        kernel().finish(ctx, 0u);
    });
    hle.add("sceRtc", "sceRtcGetTime_t", [](Runtime &rt, AllegrexContext &ctx) {
        // (const pspTime *, time_t *): seconds since 1970, as a 32-bit value
        const std::uint64_t ticks = ticks_from_civil(load_psp_time(rt.memory(), arg(ctx, 0)));
        rt.memory().store32(arg(ctx, 1), static_cast<std::uint32_t>((ticks - kUnixEpochTicks) / 1000000ull));
        kernel().finish(ctx, 0u);
    });
    hle.add("sceRtc", "sceRtcTickAddSeconds", [](Runtime &rt, AllegrexContext &ctx) { tick_add(rt, ctx, 1000000ull); });
    hle.add("sceRtc", "sceRtcTickAddMinutes", [](Runtime &rt, AllegrexContext &ctx) { tick_add(rt, ctx, 60000000ull); });
    hle.add("sceRtc", "sceRtcTickAddHours", [](Runtime &rt, AllegrexContext &ctx) { tick_add(rt, ctx, 3600000000ull); });
    hle.add("sceRtc", "sceRtcTickAddDays", [](Runtime &rt, AllegrexContext &ctx) { tick_add(rt, ctx, 86400000000ull); });
    hle.add("sceRtc", "sceRtcGetDayOfWeek", [](Runtime &, AllegrexContext &ctx) {
        // (year, month, day) -> 0 is Sunday; 1970-01-01 was a Thursday.
        const std::int64_t days = days_from_civil(static_cast<std::int32_t>(arg(ctx, 0)), arg(ctx, 1), arg(ctx, 2));
        kernel().finish(ctx, static_cast<std::uint32_t>(((days % 7) + 11) % 7));
    });
    hle.add("sceRtc", "sceRtcGetDaysInMonth", [](Runtime &, AllegrexContext &ctx) {
        static constexpr std::uint32_t kDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        const std::uint32_t month = arg(ctx, 1);
        if (month < 1u || month > 12u) {
            kernel().finish(ctx, 0x80000107u); // SCE_ERROR_INVALID_VALUE
            return;
        }
        kernel().finish(ctx, kDays[month - 1u] + (month == 2u && is_leap(arg(ctx, 0)) ? 1u : 0u));
    });
}

// ---- power, language, controls, UMD, utility, network ----------------------

void finish_constant(AllegrexContext &ctx, std::uint32_t value) { kernel().finish(ctx, value); }

void register_machine(mhp3rd::HleRegistrar &hle) {
    hle.add("ThreadManForUser", "sceKernelSysClock2USec", [](Runtime &rt, AllegrexContext &ctx) {
        // (SceKernelSysClock *clock, u32 *seconds, u32 *microseconds). A sysclock is
        // already microseconds, so this splits it.
        const std::uint64_t microseconds = load_u64(rt.memory(), arg(ctx, 0));
        if (arg(ctx, 1) != 0u) rt.memory().store32(arg(ctx, 1), static_cast<std::uint32_t>(microseconds / 1000000ull));
        if (arg(ctx, 2) != 0u) rt.memory().store32(arg(ctx, 2), static_cast<std::uint32_t>(microseconds % 1000000ull));
        kernel().finish(ctx, 0u);
    });
    hle.add("sceWlanDrv", "sceWlanGetEtherAddr", [](Runtime &rt, AllegrexContext &ctx) {
        // A locally administered address (bit 1 of the first byte set).
        static constexpr std::uint8_t kMac[6] = {0x02, 0x50, 0x53, 0x50, 0x00, 0x01};
        for (std::uint32_t i = 0; i < 6u; ++i) rt.memory().store8(arg(ctx, 0) + i, kMac[i]);
        kernel().finish(ctx, 0u);
    });

    // A retail PSP at its default speeds.
    hle.add("scePower", "scePowerGetPllClockFrequencyInt", [](Runtime &, AllegrexContext &ctx) { finish_constant(ctx, 222u); });
    hle.add("scePower", "scePowerGetCpuClockFrequencyInt", [](Runtime &, AllegrexContext &ctx) { finish_constant(ctx, 222u); });
    hle.add("scePower", "scePowerGetBusClockFrequencyInt", [](Runtime &, AllegrexContext &ctx) { finish_constant(ctx, 111u); });

    hle.add("sceImpose", "sceImposeGetLanguageMode", [](Runtime &rt, AllegrexContext &ctx) {
        // (int *language, int *confirm button): English, cross confirms
        if (arg(ctx, 0) != 0u) rt.memory().store32(arg(ctx, 0), 1u);
        if (arg(ctx, 1) != 0u) rt.memory().store32(arg(ctx, 1), 1u);
        kernel().finish(ctx, 0u);
    });
    hle.add("sceCtrl", "sceCtrlSetIdleCancelThreshold", [](Runtime &, AllegrexContext &ctx) { finish_constant(ctx, 0u); });

    hle.add("sceUmdUser", "sceUmdDeactivate", [](Runtime &, AllegrexContext &ctx) { finish_constant(ctx, 0u); });
    hle.add("sceUmdUser", "sceUmdWaitDriveStatCB", [](Runtime &, AllegrexContext &ctx) { finish_constant(ctx, 0u); });

    hle.add("sceUtility", "sceUtilityGetSystemParamString", [](Runtime &rt, AllegrexContext &ctx) {
        // (id, char *buffer, int length): the nickname is "PSP"
        mhp3rd::write_cstring(rt.memory(), arg(ctx, 1), "PSP", arg(ctx, 2));
        kernel().finish(ctx, 0u);
    });
    hle.add("sceUtility", "sceUtilityMsgDialogAbort", [](Runtime &, AllegrexContext &ctx) { finish_constant(ctx, 0u); });

    // Networking: initialises, never connects.
    hle.add("sceNetAdhocctl", "sceNetAdhocctlConnect", [](Runtime &, AllegrexContext &ctx) { finish_constant(ctx, 0u); });
    hle.add("sceNetAdhocctl", "sceNetAdhocctlGetState", [](Runtime &rt, AllegrexContext &ctx) {
        if (arg(ctx, 0) != 0u) rt.memory().store32(arg(ctx, 0), 0u); // disconnected
        kernel().finish(ctx, 0u);
    });
    hle.add("sceNetApctl", "sceNetApctlGetState", [](Runtime &rt, AllegrexContext &ctx) {
        if (arg(ctx, 0) != 0u) rt.memory().store32(arg(ctx, 0), 0u); // disconnected
        kernel().finish(ctx, 0u);
    });
    hle.add("sceNetInet", "sceNetInetInetAton", [](Runtime &, AllegrexContext &ctx) { finish_constant(ctx, 0u); }); // not an address
}

} // namespace

void register_tekken_system(mhp3rd::HleRegistrar &hle) {
    register_rtc(hle);
    register_machine(hle);
}

} // namespace tekken6
