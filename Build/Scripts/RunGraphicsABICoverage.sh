#!/usr/bin/env bash
set -euo pipefail

# LLVM 18 coverage is isolated from the FASTBuild developer and sanitizer trees.
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build="${root}/out/Linux/graphics-abi-coverage"
report="${root}/out/Verification/GraphicsABI"
mkdir -p "${report}/raw"
rm -f "${report}/raw/"*.profraw

command -v clang++-18 >/dev/null
command -v llvm-profdata-18 >/dev/null
command -v llvm-cov-18 >/dev/null

cmake -S "${root}" -B "${build}" -G Ninja \
    -DCMAKE_CXX_COMPILER=clang++-18 \
    -DCMAKE_C_COMPILER=clang-18 \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fprofile-instr-generate -fcoverage-mapping -fno-omit-frame-pointer" \
    -DCMAKE_EXE_LINKER_FLAGS="-fprofile-instr-generate" \
    -DCMAKE_SHARED_LINKER_FLAGS="-fprofile-instr-generate" \
    -DSIR_BUILD_TESTS=ON \
    -DSIR_WITH_GOOGLETEST=ON \
    -DSIR_WITH_VULKAN=OFF \
    -DSIR_WITH_D3D12=OFF

cmake --build "${build}" --parallel 2 --target \
    SIRGraphicsInterfaceTests SIRGraphicsABITests SIRGraphicsPipelineABITests \
    SIRGraphicsPipelineBridgeTests SIRGraphicsABIMockModule SIRGraphicsModuleTests \
    SIRGraphicsABIContractTests SIRGraphicsABIPropertyTests \
    SIRGraphicsABIEraMatrixTests SIRGraphicsABINegativeMatrixTests SIRGraphicsABIAssertionGoogleTests SIRGraphicsABISharedBoundaryTests

export LLVM_PROFILE_FILE="${report}/raw/%p-%m.profraw"
ctest --test-dir "${build}" --output-on-failure --no-tests=error \
    -R '^renderer[.]graphics[.]'

shopt -s nullglob
profiles=( "${report}/raw/"*.profraw )
if (( ${#profiles[@]} == 0 )); then
    echo "No instrumented LLVM profiles were generated" >&2
    exit 2
fi
llvm-profdata-18 merge -sparse "${profiles[@]}" -o "${report}/graphics.profdata"

objects=(
    "${build}/Tests/Unit/SIRGraphicsInterfaceTests"
    "${build}/Tests/Unit/SIRGraphicsABITests"
    "${build}/Tests/Unit/SIRGraphicsPipelineABITests"
    "${build}/Tests/Unit/SIRGraphicsPipelineBridgeTests"
    "${build}/Tests/Unit/SIRGraphicsModuleTests"
    "${build}/Tests/Unit/SIRGraphicsABIContractTests"
    "${build}/Tests/Unit/SIRGraphicsABIPropertyTests"
    "${build}/Tests/Unit/SIRGraphicsABIEraMatrixTests"
    "${build}/Tests/Unit/SIRGraphicsABINegativeMatrixTests"
    "${build}/Tests/Unit/SIRGraphicsABIAssertionGoogleTests"
    "${build}/Tests/Unit/SIRGraphicsABISharedBoundaryTests"
    "${build}/Tests/Unit/libSIRGraphicsABIMockModule.so"
)
arguments=()
for object in "${objects[@]}"; do
    if [[ ! -f "${object}" ]]; then
        echo "Missing instrumented ABI test object: ${object}" >&2
        exit 2
    fi
done
for (( i = 1; i < ${#objects[@]}; ++i )); do
    arguments+=( "-object=${objects[i]}" )
done

llvm-cov-18 export "${objects[0]}" "${arguments[@]}" \
    -instr-profile="${report}/graphics.profdata" \
    -format=text > "${report}/llvm-export.json"

python3 "${root}/Build/Scripts/CheckGraphicsABICoverage.py" \
    "${report}/llvm-export.json" "${root}" "${report}/coverage-summary.json"
llvm-cov-18 report "${objects[0]}" "${arguments[@]}" \
    -instr-profile="${report}/graphics.profdata" \
    "${root}/Source/Renderer" > "${report}/coverage-report.txt"
cat "${report}/coverage-summary.json"
python3 "${root}/Build/Scripts/ReportGraphicsABIGaps.py" \
    "${report}/llvm-export.json" "${root}" "${report}/uncovered-sites.json"
