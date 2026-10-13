#!/usr/bin/env bash
# Builds the profile-guided release, build/pgo (decision 0016). The instrumented build, build/pgo-gen,
# runs every reference and the programs of tests/programs at length; each argument is one more run
# to profile, a command line in which $CLI is the instrumented jagmcp-cli (a run of the BIOS and a
# cartridge kept outside the repository, say). Needs clang and its llvm-profdata.
#
#   scripts/build-pgo.sh
#   scripts/build-pgo.sh '"$CLI" --m68k --bios ~/bios.rom --cart ~/game.j64 --cycles 60000000'
set -euo pipefail
root="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
if command -v xcrun > /dev/null 2>&1; then
  profdata="$( xcrun --find llvm-profdata )" # the one of Apple's clang, whose profiles it reads
else
  profdata="$( "$root/scripts/find-llvm-tool.sh" llvm-profdata )"
fi

cd "$root"
cmake --preset pgo-gen > /dev/null
cmake --build --preset pgo-gen --target jagmcp-cli
gen="$root/build/pgo-gen"
rm -f "$gen"/*.profraw "$gen/jagmcp.profdata"
export CLI="$gen/src/jag_cli/jagmcp-cli"
export LLVM_PROFILE_FILE="$gen/run-%p.profraw"

( cd "$root/tests/reference" && "$CLI" --check-all . > /dev/null )
for program in tj_starpath tj_julia; do
  args="$( sed 's/--cycles [0-9]*/--cycles 20000000/' "$root/tests/reference/$program.args" )"
  # shellcheck disable=SC2086 # the arguments are words
  ( cd "$root/tests/reference" && "$CLI" "$program.o" $args > /dev/null )
done
for run in "$@"; do
  bash -c "$run" > /dev/null
done

unset LLVM_PROFILE_FILE
"$profdata" merge -o "$gen/jagmcp.profdata" "$gen"/*.profraw
cmake --preset pgo > /dev/null
# every object again: the build does not know the profile changed, and objects of two profiles do
# not link (ThinLTO's ProfileSummary)
cmake --build --preset pgo --clean-first
