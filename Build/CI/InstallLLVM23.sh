#!/usr/bin/env bash
set -euo pipefail

verify_versions() {
    clang++-23 --version | grep -F '23.1.5'
    llvm-cov-23 --version | grep -F '23.1.5'
    llvm-profdata-23 --version | grep -F '23.1.5'
}

if command -v clang++-23 >/dev/null 2>&1 && command -v llvm-cov-23 >/dev/null 2>&1 &&
   command -v llvm-profdata-23 >/dev/null 2>&1; then
    verify_versions
    exit 0
fi

root="${RUNNER_TEMP:-/tmp}/sir-llvm23"
mkdir -p "$root"
curl -fsSL --retry 3 https://apt.llvm.org/llvm-snapshot.gpg.key -o "$root/key.asc"
fingerprint="$(gpg --show-keys --with-colons "$root/key.asc" | awk -F: '$1 == "fpr" {print $10; exit}')"
test "$fingerprint" = 6084F3CF814B57C1CF12EFD515CF4D18AF4F7421
gpg --dearmor < "$root/key.asc" > "$root/key.gpg"
sudo install -D -m 0644 "$root/key.gpg" /etc/apt/keyrings/sir-llvm-23.gpg
echo 'deb [signed-by=/etc/apt/keyrings/sir-llvm-23.gpg] https://apt.llvm.org/noble/ llvm-toolchain-noble-23 main' | sudo tee /etc/apt/sources.list.d/sir-llvm-23.list
sudo apt-get update
sudo apt-get install -y clang-23 llvm-23 libclang-rt-23-dev ninja-build
verify_versions
