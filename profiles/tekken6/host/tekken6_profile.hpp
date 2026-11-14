#pragma once

#include "psprecomp/elf32.hpp"
#include "psprecomp/runtime.hpp"

#include <cstdint>
#include <filesystem>

namespace tekken6 {

// The kernel's allocator assumes the 64 MiB layout (kUserMemoryEnd), even
// though Tekken's image fits in 32 MiB, so the guest is created with 64 MiB.
inline constexpr std::uint32_t kGuestRamBytes = 64u * 1024u * 1024u;
inline constexpr std::uint32_t kLoadBase = psprecomp::kDefaultPspUserLoadBase;

// SHA-256 of the decrypted executable this profile was written against.
inline constexpr const char *kExecutableSha256 = "e0ed418952c51032796ebc54584592a9f6075495229f6d12d5065f3186bcb55f";

struct ProfilePaths {
    std::filesystem::path disc_image;   // UMD ISO; empty disables disc0:
    std::filesystem::path memory_stick; // host directory backing ms0:
};

// Installs the kernel and the firmware modules Tekken uses, binds logging stubs
// for the imports that are not implemented yet, and prepares the loader thread
// that runs module_start.
void install_profile(psprecomp::Runtime &runtime, const psprecomp::Elf32Image &elf, const ProfilePaths &paths);

} // namespace tekken6
