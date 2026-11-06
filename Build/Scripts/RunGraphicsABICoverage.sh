#!/usr/bin/env bash
set -euo pipefail
# Graphics ABI coverage shares the canonical LLVM 23 production qualification.
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$root"
export SIR_QUALIFICATION_SCOPE=GraphicsABI
exec bash Build/Scripts/RunLLVM23Qualification.sh
