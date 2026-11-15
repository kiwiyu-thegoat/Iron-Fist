# Tekken 6 (ULUS-10466)

A Yakumo-style profile for the PSP version of Tekken 6. **Early stage.** The
executable is recompiled to C++, the generated code compiles, and a headless
bring-up host runs it through startup and graphics initialization to the point
where it asks for its data files (`disc0:/PSP_GAME/USRDIR/DATA401.IDX`). There is
no renderer, audio or input yet, so nothing is drawn or played.

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

## The headless host (`host/`)

`tekken6_headless <EBOOT.ELF> [disc.iso] [max_dispatches]` runs the recompiled
game with no window. It shares the mhp3rd kernel and its thread, memory, file and
system firmware code in place (no copy), plus:

- `hle_tekken.cpp` waiting on a thread to end, lightweight mutexes, virtual
  timers, fixed-size pools, module loading
- `hle_tekken_system.cpp` the clock (sceRtc), clock speeds, language, network
  state (always "not connected")
- `hle_tekken_gpu.cpp` the display and GE with no drawing: the real video memory
  base, vblank waits, lists that finish at once
- `shims.cpp` "feature off" answers for mhp3rd things Tekken does not have

Any import with no handler gets a logging stub that prints its first call, so the
log is the to-do list. Useful switches: `TEKKEN6_WATCHDOG=<seconds>` reports where
the guest is if it is still running, `PSPRECOMP_HLE_HISTOGRAM=1` counts firmware
calls, `TEKKEN6_DUMP_THUNKS=1` prints a few import thunks.

Gotchas found on the way: (1) `psp_recomp` must see the module info or it emits
no import wrappers (rebuild the framework tools after the `src/elf32.cpp`
change); (2) never pipe `psp_recomp` through `head`, it is killed partway and
leaves a half-regenerated corpus.

## Where it stands with game data

With the files from `PSP_GAME/USRDIR` packed into a test ISO
(`scripts/make_test_iso.py <USRDIR> out.iso`), the game opens `DATA401.IDX`, asks
for its start sector (the host answers `sceIoIoctl` 0x01020006), reads the
120-byte `DINF_400.BIN`, creates six threads, then stops in its request-pool code
on a null pointer that real hardware would not reach. The big archive
`DATA401.BIN` (`DINF_400.BIN` gives its size as 0x16E42800, about 366 MiB) was not
supplied, so it is not yet known whether that is a host bug or the game taking an
error path. Debug switches: `MHP3RD_TRACE_IO=1`, `TEKKEN6_DUMP="0xADDR:0xLEN,..."`,
`TEKKEN6_TRACE_MUTEX=1`, `PSPRECOMP_TRACE_ON_ERROR=1`.

The shared `mhp3rd/host/hle/hle_io.cpp` gains one handler, `sceIoIoctl`, used only
when a title asks for a UMD file's start sector.

## Still to do

- The disc: put your ISO's contents where `game_root` points, or pass the ISO.
- Imports with no handler yet (the stub log shows the first ones): `sceDisplay`
  hcount, `sceWlanGetEtherAddr`, a SetCompiledSdkVersion variant, cache-range calls.
- Everything visible and audible: a renderer (the mhp3rd Vulkan renderer and GE
  state tracker are the starting point), audio (SAS and ATRAC3+), input, PSMF movies.
- 60 fps pacing (mhp3rd runs at 30) and the 887 VFPU-heavy functions' correctness.
