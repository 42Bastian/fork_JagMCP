#!/usr/bin/env bash
# Builds the owner's JagMCP: profile-guided with Homebrew's LLVM (build-pgo.sh --brew), a run of a
# cartridge through the BIOS among the runs profiled, the suite run, and the two binaries installed.
# The build directories of Homebrew's LLVM are removed first when its version changed since they
# were made, so every object is compiled by the new compiler.
#
#   scripts/build-bs42.sh            # build, test, install
#   scripts/build-bs42.sh --clean    # the same, from fresh build directories
#   scripts/build-bs42.sh --no-install
#
# Overridable: JAG_BIOS (the BIOS), JAG_PROFILE_CART (the cartridge profiled; empty for none),
# JAG_BINDIR (where the binaries go).
set -euo pipefail
root="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
bios="${JAG_BIOS:-$HOME/jaguar/new_bjl/bin/jagboot_model_m.rom}"
cart="${JAG_PROFILE_CART-$HOME/jaguar/slope/breakout/bin/breakout.j64}"
bindir="${JAG_BINDIR:-$HOME/bin}"

clean=false
install=true
for arg in "$@"; do
  case "$arg" in
  --clean) clean=true ;;
  --no-install) install=false ;;
  *)
    echo "usage: $0 [--clean] [--no-install]" >&2
    exit 2
    ;;
  esac
done

cd "$root"
llvm="$( brew --prefix llvm )"
llvmVersion="$( "$llvm/bin/clang" --version | head -n 1 )"
stamp="$root/build/pgo-brew/llvm-version"
if [[ -f "$stamp" && "$( cat "$stamp" )" != "$llvmVersion" ]]; then
  echo "Homebrew's LLVM changed ($( cat "$stamp" ) -> $llvmVersion): building from scratch"
  clean=true
fi
if $clean; then
  rm -rf build/pgo-gen-brew build/pgo-brew build/release-brew
fi

version="$( git describe --tags --dirty --match 'v[0-9]*' 2> /dev/null || echo devel )"
echo "Building JagMCP ${version#v} with $llvmVersion"
case "$version" in
*-dirty) echo "warning: uncommitted changes, the version says so" >&2 ;;
*-g*) echo "warning: commits since the last tag; tag vX.Y.Z-BS42.N for a clean version" >&2 ;;
esac

runs=()
if [[ -n "$cart" ]]; then
  if [[ -f "$bios" && -f "$cart" ]]; then
    runs+=( "\"\$CLI\" --m68k --bios \"$bios\" --cart \"$cart\" --cycles 60000000" )
  else
    echo "warning: no BIOS or cartridge to profile ($bios, $cart); the references alone" >&2
  fi
fi
./scripts/build-pgo.sh --brew ${runs[@]+"${runs[@]}"} # bash 3.2 has no empty array under set -u
echo "$llvmVersion" > "$stamp"

ctest --preset pgo-brew
build/pgo-brew/src/jag_cli/jagmcp-cli --check-all tests/reference | tail -n 1

if $install; then
  mkdir -p "$bindir"
  # removed first: macOS kills a binary copied over a signed one in place (the old signature is
  # cached by the file, which a copy keeps)
  rm -f "$bindir/jagmcp" "$bindir/jagmcp-cli"
  cp build/pgo-brew/src/jag_app/jagmcp build/pgo-brew/src/jag_cli/jagmcp-cli "$bindir/"
  echo "Installed $( "$bindir/jagmcp-cli" --version ) to $bindir"
fi
