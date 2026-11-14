#!/usr/bin/env bash
# Check the executable, analyze it and generate the AOT corpus.
#
#   generate.sh [framework_build_dir]
#
# Needs profiles/tekken6/game/EBOOT.ELF (the decrypted executable from your own
# copy) and a framework build with psp_analyze and psp_recomp in the build
# directory (default out/framework). Output: analysis/ and generated/, both
# ignored by Git.
set -euo pipefail

profile_dir="$(cd "$(dirname "$0")/.." && pwd)"
repo_dir="$(cd "$profile_dir/../.." && pwd)"
build_dir="${1:-$repo_dir/out/framework}"
elf="$profile_dir/game/EBOOT.ELF"
expected="$(sed -n 's/^expected_sha256 *= *"\(.*\)"/\1/p' "$profile_dir/config/tekken6_ulus10466.toml")"

if [[ ! -f "$elf" ]]; then
    echo "missing $elf" >&2
    echo "copy your decrypted EBOOT there and name it EBOOT.ELF" >&2
    exit 1
fi

actual="$(sha256sum "$elf" | cut -d' ' -f1)"
if [[ "$actual" != "$expected" ]]; then
    echo "unexpected executable: sha256 $actual" >&2
    echo "this profile expects            $expected" >&2
    exit 1
fi

for tool in psp_analyze psp_recomp; do
    if [[ ! -x "$build_dir/$tool" ]]; then
        echo "missing $build_dir/$tool; build the framework first:" >&2
        echo "  cmake -S . -B out/framework -DPSPRECOMP_PROFILE=\"\" -G Ninja" >&2
        echo "  cmake --build out/framework --target psp_analyze psp_recomp" >&2
        exit 1
    fi
done

mkdir -p "$profile_dir/analysis"
"$build_dir/psp_analyze" "$elf" "$profile_dir/analysis/report.json"
# psp_recomp rewrites only the units whose text changed.
"$build_dir/psp_recomp" "$elf" --auto "$profile_dir/generated"
