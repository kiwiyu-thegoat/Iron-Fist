// Headless Tekken 6 (ULUS-10466) bring-up host.
//
//   tekken6_headless <EBOOT.ELF> [disc.iso] [max_dispatches]
//
// Loads the decrypted executable, installs the shared kernel and the firmware
// modules, and runs the recompiled game with no window, audio or input. It
// exists to find out, call by call, what the game needs from the host.

#include "tekken6_profile.hpp"

#include "kernel/kernel.hpp"

#include "psprecomp/common.hpp"
#include "psprecomp/elf32.hpp"
#include "psprecomp/runtime.hpp"
#include "psprecomp/sha256.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <chrono>
#include <string>
#include <thread>

#ifndef PSPRECOMP_DEFAULT_NIDS_CSV
#define PSPRECOMP_DEFAULT_NIDS_CSV "configs/nids.csv"
#endif

int main(int argc, char **argv) {
    try {
        if (argc < 2) {
            std::cerr << "usage: " << argv[0] << " <EBOOT.ELF> [disc.iso] [max_dispatches]\n";
            return 2;
        }
        const std::filesystem::path executable = argv[1];
        tekken6::ProfilePaths paths;
        if (argc > 2) paths.disc_image = argv[2];
        paths.memory_stick = "tekken6_data/ms0";
        std::filesystem::create_directories(paths.memory_stick);
        const std::uint64_t max_dispatches = argc > 3 ? std::strtoull(argv[3], nullptr, 10) : 200'000'000ull;

        const std::string sha256 = psprecomp::sha256_file(executable);
        if (sha256 != tekken6::kExecutableSha256) std::cerr << "warning: unsupported executable hash " << sha256 << "\n";

        const psprecomp::Elf32Image elf = psprecomp::Elf32Image::from_file(executable);
        psprecomp::Runtime runtime(tekken6::kGuestRamBytes);
        const char *nids = std::getenv("TEKKEN6_NIDS");
        runtime.nids().load_csv(nids ? nids : PSPRECOMP_DEFAULT_NIDS_CSV);
        (void)elf.load_and_relocate(runtime.memory(), tekken6::kLoadBase);
        psprecomp::register_generated_functions(runtime);
        tekken6::install_profile(runtime, elf, paths);

        std::cout << "Tekken 6 headless bring-up\n"
                  << "Executable: " << executable.string() << "\n"
                  << "SHA-256:    " << sha256 << "\n"
                  << "Disc image: " << (paths.disc_image.empty() ? "<none>" : paths.disc_image.string()) << "\n"
                  << "Entry:      " << psprecomp::hex32(elf.runtime_entry(tekken6::kLoadBase)) << "\n"
                  << "Functions:  " << runtime.function_count() << "\n";
        if (runtime.function_count() == 0u) {
            std::cout << "No generated functions are linked. Run profiles/tekken6/scripts/generate.sh and rebuild.\n";
            return 3;
        }

        if (std::getenv("TEKKEN6_DUMP_THUNKS") != nullptr) {
            for (std::uint32_t address : {0x08B88DD0u, 0x08B88DE8u, 0x08B88EC0u})
                std::cerr << "[thunk] " << psprecomp::hex32(address) << ": " << psprecomp::hex32(runtime.memory().load32(address))
                          << " " << psprecomp::hex32(runtime.memory().load32(address + 4u)) << "\n";
        }
        // Diagnostic: TEKKEN6_WATCHDOG=<seconds> reports where the guest is if it is
        // still running after that long, then exits. The read is unsynchronised,
        // which is fine for a one-off look at a program that is not making progress.
        if (const char *seconds = std::getenv("TEKKEN6_WATCHDOG")) {
            const auto limit = std::chrono::seconds(std::strtoul(seconds, nullptr, 10));
            std::thread([&runtime, limit]() {
                std::this_thread::sleep_for(limit);
                const auto &cpu = runtime.cpu();
                std::cerr << "[watchdog] pc=" << psprecomp::hex32(cpu.pc) << " ra=" << psprecomp::hex32(cpu.gpr[31])
                          << " sp=" << psprecomp::hex32(cpu.gpr[29]) << " gp=" << psprecomp::hex32(cpu.gpr[28])
                          << " a0=" << psprecomp::hex32(cpu.gpr[4]) << " a1=" << psprecomp::hex32(cpu.gpr[5]) << "\n"
                          << mhp3rd::kernel().describe_threads() << std::endl;
                runtime.report_hle_histogram();
                std::_Exit(5);
            }).detach();
        }
        runtime.run(elf.runtime_entry(tekken6::kLoadBase), max_dispatches);
        std::cout << "Runtime stopped: " << runtime.stop_reason() << "\n";
        std::cout << mhp3rd::kernel().describe_threads() << "\n";
        runtime.report_hle_histogram();
        return runtime.stop_reason().empty() ? 0 : 4;
    } catch (const std::exception &e) {
        std::cerr << "Tekken 6 error: " << e.what() << "\n";
        return 1;
    }
}
