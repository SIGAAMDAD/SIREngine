#!/usr/bin/env bash
set -euo pipefail

# Routine Clang smoke builds track LLVM major 23; qualification is pinned to 23.1.5.
mode="${SIR_LLVM_MODE:-qualification}"
if [[ "$RUNNER_OS" != Linux ]]; then
    echo "LLVM 23 setup supports Linux x64 runners only" >&2
    exit 1
fi

verify_pr() {
    clang++-23 --version | grep -E 'version 23[.]'
    llvm-cov-23 --version | grep -E 'version 23[.]'
    llvm-profdata-23 --version | grep -E 'version 23[.]'
}
verify_exact() {
    clang++-23 --version | grep -F '23.1.5'
    llvm-cov-23 --version | grep -F '23.1.5'
    llvm-profdata-23 --version | grep -F '23.1.5'
}

if [[ "$mode" == pr ]]; then
    if command -v clang++-23 >/dev/null 2>&1 &&
       command -v llvm-cov-23 >/dev/null 2>&1 &&
       command -v llvm-profdata-23 >/dev/null 2>&1; then
        verify_pr
        exit 0
    fi
    root="${RUNNER_TEMP:-/tmp}/sir-llvm23-pr"
    mkdir -p "$root"
    curl -fsSL --retry 3 https://apt.llvm.org/llvm-snapshot.gpg.key -o "$root/key.asc"
    fingerprint="$(gpg --show-keys --with-colons "$root/key.asc" | awk -F: '$1 == "fpr" {print $10; exit}')"
    test "$fingerprint" = 6084F3CF814B57C1CF12EFD515CF4D18AF4F7421
    gpg --dearmor < "$root/key.asc" > "$root/key.gpg"
    sudo install -D -m 0644 "$root/key.gpg" /etc/apt/keyrings/sir-llvm-23.gpg
    echo 'deb [signed-by=/etc/apt/keyrings/sir-llvm-23.gpg] https://apt.llvm.org/noble/ llvm-toolchain-noble-23 main' | sudo tee /etc/apt/sources.list.d/sir-llvm-23.list
    sudo apt-get update
    sudo apt-get install -y clang-23 llvm-23 libclang-rt-23-dev ninja-build
    verify_pr
    exit 0
fi
if [[ "$mode" != qualification ]]; then
    echo "Unknown SIR_LLVM_MODE: $mode" >&2
    exit 2
fi
if command -v clang++-23 >/dev/null 2>&1 &&
   command -v llvm-cov-23 >/dev/null 2>&1 &&
   command -v llvm-profdata-23 >/dev/null 2>&1 &&
   clang++-23 --version | grep -Fq '23.1.5' &&
   llvm-cov-23 --version | grep -Fq '23.1.5' &&
   llvm-profdata-23 --version | grep -Fq '23.1.5'; then
    verify_exact
    exit 0
fi

root="${RUNNER_TEMP:-/tmp}/sir-llvm-23.1.5"
archive="$root/LLVM-23.1.5-Linux-X64.tar.zst"
digest=c3107384835f788768f614db610e87fba7077f125ca950eb02a78bab0a308065
mkdir -p "$root/unpack" "$root/toolbin"
if [[ ! -f "$archive" ]]; then
    curl -fL --retry 3 --retry-delay 5 --connect-timeout 20 \
        https://github.com/llvm/llvm-project/releases/download/llvmorg-23.1.5/LLVM-23.1.5-Linux-X64.tar.zst \
        -o "$archive"
fi
printf '%s  %s\n' "$digest" "$archive" | sha256sum -c -
if ! command -v zstd >/dev/null 2>&1; then
    sudo apt-get update
    sudo apt-get install -y zstd
fi
if ! command -v ninja >/dev/null 2>&1; then
    sudo apt-get install -y ninja-build
fi
tar --use-compress-program='zstd --long=30' -xf "$archive" -C "$root/unpack"
bin="$(find "$root/unpack" -path '*/bin/clang++' -print -quit)"
test -n "$bin"
bin="$(dirname "$bin")"
for tool in clang clang++ llvm-cov llvm-profdata; do
    test -x "$bin/$tool"
done
ln -sf "$bin/clang" "$root/toolbin/clang-23"
ln -sf "$bin/clang++" "$root/toolbin/clang++-23"
ln -sf "$bin/llvm-cov" "$root/toolbin/llvm-cov-23"
ln -sf "$bin/llvm-profdata" "$root/toolbin/llvm-profdata-23"
printf '%s\n' "$root/toolbin" >> "$GITHUB_PATH"
export PATH="$root/toolbin:$PATH"
verify_exact
