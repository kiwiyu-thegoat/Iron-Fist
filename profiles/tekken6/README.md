# Tekken 6 (ULUS-10466)

A Yakumo-style profile for the PSP version of Tekken 6. **Early stage:** the
executable is analyzed and recompiled to C++, and the generated code compiles.
There is no host yet (kernel, graphics, audio, input), so nothing runs.

This profile contains no game files. You supply the decrypted executable from
your own copy; everything generated from it stays on your machine and is ignored
by Git.

## What is known about the executable

| | |
|---|---|
| Module | `tekken`, ELF32 MIPS, one RWX load segment, **no section headers** |
| Image | `0x08804018` to `0x08C63044` (end includes BSS) |
| Code | `0x08804018` to `0x08B894C8`; import thunks at its end, data after |
| Entry / `$gp` | `0x08804124` / `0x08C022B0` |
| Imports | 268 functions in 30 libraries |
| Decrypted SHA-256 | `e0ed418952c51032796ebc54584592a9f6075495229f6d12d5065f3186bcb55f` |

The framework change in `src/elf32.cpp` (module info found through the first
program header when there are no sections) is what makes the imports visible.
Before it, the analyzer reported no module and no imports.

## Generate (works in a browser: GitHub Codespaces)

1. Open your fork, Code, Codespaces, Create. Pick 4 cores if offered.
2. Upload your decrypted EBOOT to `profiles/tekken6/game/EBOOT.ELF`.
3. In the terminal:

```bash
cmake -S . -B out/framework -DPSPRECOMP_PROFILE="" -G Ninja
cmake --build out/framework --target psp_analyze psp_recomp
profiles/tekken6/scripts/generate.sh
cmake -S . -B out/tekken6 -DPSPRECOMP_PROFILE=tekken6 -G Ninja
cmake --build out/tekken6 --target tekken6_generated
```

Measured on a one-core machine: analysis about 1 second, code generation about
1 minute (254 units, 259 MB of C++), about 9 seconds to compile one unit.

## Host work still to do

The mhp3rd host already handles about 142 of Tekken's 268 imports (rough count
by name and NID). Still missing:

- `scePsmfPlayer` (14), the movie player; mhp3rd uses `sceMpeg` instead
- `sceRtc` (11 more), `sceUmdUser` (4), `scePower` (4), `sceDisplay` (3)
- `sceNetApctl` (7), `sceNetInet` (3), network state calls
- the `sceSasCore` calls (27) need checking against the host's mixer
- 13 `ThreadManForUser`, 13 `IoFileMgrForUser` (async and filesystem calls)

Then the profile needs its own main, configuration, a decision on how much of
the mhp3rd renderer, audio and input code to share, and 60 fps pacing (mhp3rd
runs at 30).
