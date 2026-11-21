/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef SIR_CORE_PLATFORM_PLATFORM_H
#define SIR_CORE_PLATFORM_PLATFORM_H

#include <Core/Platform/SIRCompiler.h>
#include <Core/SIRPCH.h>

// clang-format off
#if SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on

// ===========================================================================
// OS family selection. Target OS is distinct from host OS and API support.
// Platform presence does not imply an implemented SIREngine native backend.
// ===========================================================================
#if defined( __EMSCRIPTEN__ )
#define SIR_PLATFORM_EMSCRIPTEN 1
#elif defined( _WIN32 )
#define SIR_TARGET_OS_WINDOWS 1
#elif defined( __ANDROID__ )
#define SIR_PLATFORM_ANDROID 1
#elif defined( __APPLE__ ) && defined( __MACH__ )
#include <TargetConditionals.h>
#if defined( TARGET_OS_VISION ) && TARGET_OS_VISION
#define SIR_PLATFORM_VISIONOS 1
#elif defined( TARGET_OS_WATCH ) && TARGET_OS_WATCH
#define SIR_PLATFORM_WATCHOS 1
#elif defined( TARGET_OS_TV ) && TARGET_OS_TV
#define SIR_PLATFORM_TVOS 1
#elif defined( TARGET_OS_MACCATALYST ) && TARGET_OS_MACCATALYST
#define SIR_PLATFORM_MACCATALYST 1
#define SIR_PLATFORM_IOS 1
#elif defined( TARGET_OS_IPHONE ) && TARGET_OS_IPHONE
#define SIR_PLATFORM_IOS 1
#else
#define SIR_PLATFORM_MACOS 1
#endif
#elif defined( __linux__ )
#define SIR_TARGET_OS_LINUX 1
#elif defined( __FreeBSD__ )
#define SIR_PLATFORM_FREEBSD 1
#elif defined( __OpenBSD__ )
#define SIR_PLATFORM_OPENBSD 1
#elif defined( __NetBSD__ )
#define SIR_PLATFORM_NETBSD 1
#elif defined( __DragonFly__ )
#define SIR_PLATFORM_DRAGONFLY 1
#else
#define SIR_PLATFORM_UNKNOWN 1
#endif
#ifndef SIR_PLATFORM_EMSCRIPTEN
#define SIR_PLATFORM_EMSCRIPTEN 0
#endif
#ifndef SIR_TARGET_OS_WINDOWS
#define SIR_TARGET_OS_WINDOWS 0
#endif
#ifndef SIR_PLATFORM_ANDROID
#define SIR_PLATFORM_ANDROID 0
#endif
#ifndef SIR_PLATFORM_MACOS
#define SIR_PLATFORM_MACOS 0
#endif
#ifndef SIR_PLATFORM_IOS
#define SIR_PLATFORM_IOS 0
#endif
#ifndef SIR_PLATFORM_MACCATALYST
#define SIR_PLATFORM_MACCATALYST 0
#endif
#ifndef SIR_PLATFORM_TVOS
#define SIR_PLATFORM_TVOS 0
#endif
#ifndef SIR_PLATFORM_WATCHOS
#define SIR_PLATFORM_WATCHOS 0
#endif
#ifndef SIR_PLATFORM_VISIONOS
#define SIR_PLATFORM_VISIONOS 0
#endif
#ifndef SIR_TARGET_OS_LINUX
#define SIR_TARGET_OS_LINUX 0
#endif
#ifndef SIR_PLATFORM_FREEBSD
#define SIR_PLATFORM_FREEBSD 0
#endif
#ifndef SIR_PLATFORM_OPENBSD
#define SIR_PLATFORM_OPENBSD 0
#endif
#ifndef SIR_PLATFORM_NETBSD
#define SIR_PLATFORM_NETBSD 0
#endif
#ifndef SIR_PLATFORM_DRAGONFLY
#define SIR_PLATFORM_DRAGONFLY 0
#endif
#ifndef SIR_PLATFORM_UNKNOWN
#define SIR_PLATFORM_UNKNOWN 0
#endif

#define SIR_PLATFORM_BSD ( SIR_PLATFORM_FREEBSD || SIR_PLATFORM_OPENBSD || SIR_PLATFORM_NETBSD || SIR_PLATFORM_DRAGONFLY )
#define SIR_PLATFORM_APPLE ( SIR_PLATFORM_MACOS || SIR_PLATFORM_IOS || SIR_PLATFORM_TVOS || SIR_PLATFORM_WATCHOS || SIR_PLATFORM_VISIONOS )
#define SIR_PLATFORM_POSIX ( SIR_TARGET_OS_LINUX || SIR_PLATFORM_ANDROID || SIR_PLATFORM_APPLE || SIR_PLATFORM_BSD )
#define SIR_PLATFORM_UNIX ( SIR_PLATFORM_POSIX )
#define SIR_PLATFORM_DESKTOP                                                                                                               \
	( SIR_TARGET_OS_WINDOWS || SIR_TARGET_OS_LINUX || SIR_PLATFORM_MACOS || SIR_PLATFORM_MACCATALYST || SIR_PLATFORM_BSD )
#define SIR_PLATFORM_MOBILE ( SIR_PLATFORM_ANDROID || ( SIR_PLATFORM_IOS && !SIR_PLATFORM_MACCATALYST ) )
#define SIR_PLATFORM_WEB SIR_PLATFORM_EMSCRIPTEN
#define SIR_PLATFORM_NATIVE_BACKEND ( SIR_TARGET_OS_WINDOWS || SIR_TARGET_OS_LINUX )

#if SIR_TARGET_OS_WINDOWS
#define SIR_PLATFORM_NAME "Windows"
#elif SIR_PLATFORM_ANDROID
#define SIR_PLATFORM_NAME "Android"
#elif SIR_PLATFORM_MACCATALYST
#define SIR_PLATFORM_NAME "Mac Catalyst"
#elif SIR_PLATFORM_IOS
#define SIR_PLATFORM_NAME "iOS"
#elif SIR_PLATFORM_TVOS
#define SIR_PLATFORM_NAME "tvOS"
#elif SIR_PLATFORM_WATCHOS
#define SIR_PLATFORM_NAME "watchOS"
#elif SIR_PLATFORM_VISIONOS
#define SIR_PLATFORM_NAME "visionOS"
#elif SIR_PLATFORM_MACOS
#define SIR_PLATFORM_NAME "macOS"
#elif SIR_TARGET_OS_LINUX
#define SIR_PLATFORM_NAME "Linux"
#elif SIR_PLATFORM_FREEBSD
#define SIR_PLATFORM_NAME "FreeBSD"
#elif SIR_PLATFORM_OPENBSD
#define SIR_PLATFORM_NAME "OpenBSD"
#elif SIR_PLATFORM_NETBSD
#define SIR_PLATFORM_NAME "NetBSD"
#elif SIR_PLATFORM_DRAGONFLY
#define SIR_PLATFORM_NAME "DragonFly BSD"
#elif SIR_PLATFORM_EMSCRIPTEN
#define SIR_PLATFORM_NAME "Emscripten"
#else
#define SIR_PLATFORM_NAME "Unknown"
#endif

// Apple target metadata exists only when Apple's SDK exposes it.
#if SIR_PLATFORM_APPLE && defined( TARGET_OS_SIMULATOR ) && TARGET_OS_SIMULATOR
#define SIR_PLATFORM_APPLE_SIMULATOR 1
#else
#define SIR_PLATFORM_APPLE_SIMULATOR 0
#endif
#if SIR_PLATFORM_IOS && SIR_PLATFORM_APPLE_SIMULATOR
#define SIR_PLATFORM_IOS_SIMULATOR 1
#else
#define SIR_PLATFORM_IOS_SIMULATOR 0
#endif
#if SIR_TARGET_OS_WINDOWS && defined( WINAPI_FAMILY ) && defined( WINAPI_FAMILY_APP ) && ( WINAPI_FAMILY == WINAPI_FAMILY_APP )
#define SIR_TARGET_OS_WINDOWS_APP 1
#else
#define SIR_TARGET_OS_WINDOWS_APP 0
#endif
#if SIR_TARGET_OS_WINDOWS && !SIR_TARGET_OS_WINDOWS_APP
#define SIR_TARGET_OS_WINDOWS_DESKTOP 1
#else
#define SIR_TARGET_OS_WINDOWS_DESKTOP 0
#endif
#if SIR_TARGET_OS_WINDOWS && defined( _WIN32_WINNT )
#define SIR_PLATFORM_WIN32_WINNT _WIN32_WINNT
#else
#define SIR_PLATFORM_WIN32_WINNT 0
#endif
#if SIR_PLATFORM_ANDROID && defined( __ANDROID_API__ )
#define SIR_PLATFORM_ANDROID_API __ANDROID_API__
#else
#define SIR_PLATFORM_ANDROID_API 0
#endif
#if SIR_TARGET_OS_LINUX && defined( __GLIBC__ )
#define SIR_PLATFORM_GLIBC 1
#define SIR_PLATFORM_GLIBC_MAJOR __GLIBC__
#define SIR_PLATFORM_GLIBC_MINOR __GLIBC_MINOR__
#else
#define SIR_PLATFORM_GLIBC 0
#define SIR_PLATFORM_GLIBC_MAJOR 0
#define SIR_PLATFORM_GLIBC_MINOR 0
#endif

// ===========================================================================
// CPU architecture. The EC flag is a subtype of ARM64, not a second CPU ISA.
// ===========================================================================
#if defined( _M_ARM64EC )
#define SIR_CPU_ARCH_ARM64 1
#define SIR_CPU_ARCH_ARM64EC 1
#elif defined( _M_ARM64 ) || defined( __aarch64__ ) || defined( __arm64__ )
#define SIR_CPU_ARCH_ARM64 1
#elif defined( _M_X64 ) || defined( _M_AMD64 ) || defined( __x86_64__ ) || defined( __amd64__ )
#define SIR_CPU_ARCH_X64 1
#elif defined( _M_IX86 ) || defined( __i386__ )
#define SIR_CPU_ARCH_X86 1
#elif defined( _M_ARM ) || defined( __arm__ )
#define SIR_CPU_ARCH_ARM32 1
#elif defined( __riscv ) && defined( __riscv_xlen ) && __riscv_xlen == 64
#define SIR_CPU_ARCH_RISCV64 1
#elif defined( __riscv ) && defined( __riscv_xlen ) && __riscv_xlen == 32
#define SIR_CPU_ARCH_RISCV32 1
#elif defined( __powerpc64__ ) || defined( __ppc64__ )
#define SIR_CPU_ARCH_PPC64 1
#elif defined( __powerpc__ ) || defined( __ppc__ )
#define SIR_CPU_ARCH_PPC32 1
#elif defined( __wasm64__ )
#define SIR_CPU_ARCH_WASM64 1
#elif defined( __wasm32__ )
#define SIR_CPU_ARCH_WASM32 1
#elif defined( __loongarch64 )
#define SIR_CPU_ARCH_LOONGARCH64 1
#elif defined( __s390x__ )
#define SIR_CPU_ARCH_S390X 1
#else
#define SIR_CPU_ARCH_UNKNOWN 1
#endif
#ifndef SIR_CPU_ARCH_X86
#define SIR_CPU_ARCH_X86 0
#endif
#ifndef SIR_CPU_ARCH_X64
#define SIR_CPU_ARCH_X64 0
#endif
#ifndef SIR_CPU_ARCH_ARM32
#define SIR_CPU_ARCH_ARM32 0
#endif
#ifndef SIR_CPU_ARCH_ARM64
#define SIR_CPU_ARCH_ARM64 0
#endif
#ifndef SIR_CPU_ARCH_ARM64EC
#define SIR_CPU_ARCH_ARM64EC 0
#endif
#ifndef SIR_CPU_ARCH_RISCV32
#define SIR_CPU_ARCH_RISCV32 0
#endif
#ifndef SIR_CPU_ARCH_RISCV64
#define SIR_CPU_ARCH_RISCV64 0
#endif
#ifndef SIR_CPU_ARCH_PPC32
#define SIR_CPU_ARCH_PPC32 0
#endif
#ifndef SIR_CPU_ARCH_PPC64
#define SIR_CPU_ARCH_PPC64 0
#endif
#ifndef SIR_CPU_ARCH_WASM32
#define SIR_CPU_ARCH_WASM32 0
#endif
#ifndef SIR_CPU_ARCH_WASM64
#define SIR_CPU_ARCH_WASM64 0
#endif
#ifndef SIR_CPU_ARCH_LOONGARCH64
#define SIR_CPU_ARCH_LOONGARCH64 0
#endif
#ifndef SIR_CPU_ARCH_S390X
#define SIR_CPU_ARCH_S390X 0
#endif
#ifndef SIR_CPU_ARCH_UNKNOWN
#define SIR_CPU_ARCH_UNKNOWN 0
#endif

#if SIR_CPU_ARCH_X64
#define SIR_CPU_ARCH_NAME "x86_64"
#elif SIR_CPU_ARCH_X86
#define SIR_CPU_ARCH_NAME "x86"
#elif SIR_CPU_ARCH_ARM64EC
#define SIR_CPU_ARCH_NAME "ARM64EC"
#elif SIR_CPU_ARCH_ARM64
#define SIR_CPU_ARCH_NAME "AArch64"
#elif SIR_CPU_ARCH_ARM32
#define SIR_CPU_ARCH_NAME "ARM32"
#elif SIR_CPU_ARCH_RISCV64
#define SIR_CPU_ARCH_NAME "RISC-V 64"
#elif SIR_CPU_ARCH_RISCV32
#define SIR_CPU_ARCH_NAME "RISC-V 32"
#elif SIR_CPU_ARCH_PPC64
#define SIR_CPU_ARCH_NAME "PowerPC 64"
#elif SIR_CPU_ARCH_PPC32
#define SIR_CPU_ARCH_NAME "PowerPC 32"
#elif SIR_CPU_ARCH_WASM64
#define SIR_CPU_ARCH_NAME "WebAssembly 64"
#elif SIR_CPU_ARCH_WASM32
#define SIR_CPU_ARCH_NAME "WebAssembly 32"
#elif SIR_CPU_ARCH_LOONGARCH64
#define SIR_CPU_ARCH_NAME "LoongArch 64"
#elif SIR_CPU_ARCH_S390X
#define SIR_CPU_ARCH_NAME "s390x"
#else
#define SIR_CPU_ARCH_NAME "Unknown"
#endif

#if defined( __SIZEOF_POINTER__ )
#define SIR_PLATFORM_POINTER_BITS ( __SIZEOF_POINTER__ * 8 )
#elif defined( _WIN64 )
#define SIR_PLATFORM_POINTER_BITS 64
#elif defined( _WIN32 )
#define SIR_PLATFORM_POINTER_BITS 32
#elif SIR_CPU_ARCH_X64 || SIR_CPU_ARCH_ARM64 || SIR_CPU_ARCH_RISCV64 || SIR_CPU_ARCH_PPC64 || SIR_CPU_ARCH_WASM64 ||                       \
    SIR_CPU_ARCH_LOONGARCH64 || SIR_CPU_ARCH_S390X
#define SIR_PLATFORM_POINTER_BITS 64
#elif SIR_CPU_ARCH_X86 || SIR_CPU_ARCH_ARM32 || SIR_CPU_ARCH_RISCV32 || SIR_CPU_ARCH_PPC32 || SIR_CPU_ARCH_WASM32
#define SIR_PLATFORM_POINTER_BITS 32
#else
#error "SIREngine cannot determine the target pointer width"
#endif
#define SIR_PLATFORM_64BIT ( SIR_PLATFORM_POINTER_BITS == 64 )
#define SIR_PLATFORM_32BIT ( SIR_PLATFORM_POINTER_BITS == 32 )

// Instruction-set macros mean enabled FOR THIS TRANSLATION UNIT. Never use
// them as runtime CPU support tests; SIRCPUInfo performs runtime dispatch.
#if defined( __SSE__ ) || defined( _M_X64 ) || defined( _M_AMD64 ) || ( defined( _M_IX86_FP ) && _M_IX86_FP >= 1 )
#define SIR_CPU_ISA_SSE 1
#else
#define SIR_CPU_ISA_SSE 0
#endif
#if defined( __SSE2__ ) || defined( _M_X64 ) || defined( _M_AMD64 ) || ( defined( _M_IX86_FP ) && _M_IX86_FP >= 2 )
#define SIR_CPU_ISA_SSE2 1
#else
#define SIR_CPU_ISA_SSE2 0
#endif
#if defined( __SSE3__ )
#define SIR_CPU_ISA_SSE3 1
#else
#define SIR_CPU_ISA_SSE3 0
#endif
#if defined( __SSSE3__ )
#define SIR_CPU_ISA_SSSE3 1
#else
#define SIR_CPU_ISA_SSSE3 0
#endif
#if defined( __SSE4_1__ )
#define SIR_CPU_ISA_SSE41 1
#else
#define SIR_CPU_ISA_SSE41 0
#endif
#if defined( __SSE4_2__ )
#define SIR_CPU_ISA_SSE42 1
#else
#define SIR_CPU_ISA_SSE42 0
#endif
#if defined( __SSE4A__ )
#define SIR_CPU_ISA_SSE4A 1
#else
#define SIR_CPU_ISA_SSE4A 0
#endif
#if defined( __AVX__ )
#define SIR_CPU_ISA_AVX 1
#else
#define SIR_CPU_ISA_AVX 0
#endif
#if defined( __AVX2__ )
#define SIR_CPU_ISA_AVX2 1
#else
#define SIR_CPU_ISA_AVX2 0
#endif
#if defined( __AVX512F__ )
#define SIR_CPU_ISA_AVX512F 1
#else
#define SIR_CPU_ISA_AVX512F 0
#endif
#if defined( __AVX512BW__ )
#define SIR_CPU_ISA_AVX512BW 1
#else
#define SIR_CPU_ISA_AVX512BW 0
#endif
#if defined( __AVX512DQ__ )
#define SIR_CPU_ISA_AVX512DQ 1
#else
#define SIR_CPU_ISA_AVX512DQ 0
#endif
#if defined( __AVX512VL__ )
#define SIR_CPU_ISA_AVX512VL 1
#else
#define SIR_CPU_ISA_AVX512VL 0
#endif
#if defined( __AVX512VNNI__ )
#define SIR_CPU_ISA_AVX512VNNI 1
#else
#define SIR_CPU_ISA_AVX512VNNI 0
#endif
#if defined( __AVX512VBMI__ )
#define SIR_CPU_ISA_AVX512VBMI 1
#else
#define SIR_CPU_ISA_AVX512VBMI 0
#endif
#if defined( __AVX512VBMI2__ )
#define SIR_CPU_ISA_AVX512VBMI2 1
#else
#define SIR_CPU_ISA_AVX512VBMI2 0
#endif
#if defined( __AVX512BF16__ )
#define SIR_CPU_ISA_AVX512BF16 1
#else
#define SIR_CPU_ISA_AVX512BF16 0
#endif
#if defined( __AVX512FP16__ )
#define SIR_CPU_ISA_AVX512FP16 1
#else
#define SIR_CPU_ISA_AVX512FP16 0
#endif
#if defined( __FMA__ )
#define SIR_CPU_ISA_FMA 1
#else
#define SIR_CPU_ISA_FMA 0
#endif
#if defined( __F16C__ )
#define SIR_CPU_ISA_F16C 1
#else
#define SIR_CPU_ISA_F16C 0
#endif
#if defined( __BMI__ )
#define SIR_CPU_ISA_BMI1 1
#else
#define SIR_CPU_ISA_BMI1 0
#endif
#if defined( __BMI2__ )
#define SIR_CPU_ISA_BMI2 1
#else
#define SIR_CPU_ISA_BMI2 0
#endif
#if defined( __AES__ )
#define SIR_CPU_ISA_AES 1
#else
#define SIR_CPU_ISA_AES 0
#endif
#if defined( __PCLMUL__ )
#define SIR_CPU_ISA_PCLMUL 1
#else
#define SIR_CPU_ISA_PCLMUL 0
#endif
#if defined( __POPCNT__ )
#define SIR_CPU_ISA_POPCNT 1
#else
#define SIR_CPU_ISA_POPCNT 0
#endif
#if defined( __LZCNT__ )
#define SIR_CPU_ISA_LZCNT 1
#else
#define SIR_CPU_ISA_LZCNT 0
#endif
#if defined( __SHA__ )
#define SIR_CPU_ISA_SHA 1
#else
#define SIR_CPU_ISA_SHA 0
#endif
#if defined( __GFNI__ )
#define SIR_CPU_ISA_GFNI 1
#else
#define SIR_CPU_ISA_GFNI 0
#endif
#if defined( __VAES__ )
#define SIR_CPU_ISA_VAES 1
#else
#define SIR_CPU_ISA_VAES 0
#endif
#if defined( __ARM_NEON ) || defined( __ARM_NEON__ ) || defined( _M_ARM64 ) || defined( _M_ARM64EC )
#define SIR_CPU_ISA_NEON 1
#else
#define SIR_CPU_ISA_NEON 0
#endif
#if defined( __ARM_FEATURE_SVE )
#define SIR_CPU_ISA_SVE 1
#else
#define SIR_CPU_ISA_SVE 0
#endif
#if defined( __ARM_FEATURE_SVE2 )
#define SIR_CPU_ISA_SVE2 1
#else
#define SIR_CPU_ISA_SVE2 0
#endif
#if defined( __ARM_FEATURE_AES )
#define SIR_CPU_ISA_ARM_AES 1
#else
#define SIR_CPU_ISA_ARM_AES 0
#endif
#if defined( __ARM_FEATURE_SHA2 )
#define SIR_CPU_ISA_ARM_SHA2 1
#else
#define SIR_CPU_ISA_ARM_SHA2 0
#endif
#if defined( __ARM_FEATURE_CRC32 )
#define SIR_CPU_ISA_ARM_CRC32 1
#else
#define SIR_CPU_ISA_ARM_CRC32 0
#endif
#if defined( __ARM_FEATURE_DOTPROD )
#define SIR_CPU_ISA_ARM_DOTPROD 1
#else
#define SIR_CPU_ISA_ARM_DOTPROD 0
#endif
#if defined( __ARM_FEATURE_MATMUL_INT8 )
#define SIR_CPU_ISA_ARM_I8MM 1
#else
#define SIR_CPU_ISA_ARM_I8MM 0
#endif
#if defined( __ARM_FEATURE_BF16 )
#define SIR_CPU_ISA_ARM_BF16 1
#else
#define SIR_CPU_ISA_ARM_BF16 0
#endif
#if defined( __ARM_FEATURE_FP16_VECTOR_ARITHMETIC )
#define SIR_CPU_ISA_ARM_FP16 1
#else
#define SIR_CPU_ISA_ARM_FP16 0
#endif
#if defined( __wasm_simd128__ )
#define SIR_CPU_ISA_WASM_SIMD128 1
#else
#define SIR_CPU_ISA_WASM_SIMD128 0
#endif
#if defined( __wasm_relaxed_simd__ )
#define SIR_CPU_ISA_WASM_RELAXED_SIMD 1
#else
#define SIR_CPU_ISA_WASM_RELAXED_SIMD 0
#endif
#if defined( __riscv_vector )
#define SIR_CPU_ISA_RISCV_VECTOR 1
#else
#define SIR_CPU_ISA_RISCV_VECTOR 0
#endif
#if defined( __ALTIVEC__ )
#define SIR_CPU_ISA_ALTIVEC 1
#else
#define SIR_CPU_ISA_ALTIVEC 0
#endif
#if defined( __VSX__ )
#define SIR_CPU_ISA_VSX 1
#else
#define SIR_CPU_ISA_VSX 0
#endif

#if defined( __BYTE_ORDER__ ) && defined( __ORDER_LITTLE_ENDIAN__ ) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define SIR_ENDIAN_LITTLE 1
#define SIR_ENDIAN_BIG 0
#elif defined( __BYTE_ORDER__ ) && defined( __ORDER_BIG_ENDIAN__ ) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define SIR_ENDIAN_LITTLE 0
#define SIR_ENDIAN_BIG 1
#elif defined( __ARMEL__ ) || defined( __AARCH64EL__ ) || defined( __MIPSEL__ )
#define SIR_ENDIAN_LITTLE 1
#define SIR_ENDIAN_BIG 0
#elif defined( __ARMEB__ ) || defined( __AARCH64EB__ ) || defined( __MIPSEB__ )
#define SIR_ENDIAN_LITTLE 0
#define SIR_ENDIAN_BIG 1
#elif SIR_TARGET_OS_WINDOWS || SIR_CPU_ARCH_X86 || SIR_CPU_ARCH_X64 || SIR_CPU_ARCH_WASM32 || SIR_CPU_ARCH_WASM64
#define SIR_ENDIAN_LITTLE 1
#define SIR_ENDIAN_BIG 0
#else
#error "SIREngine cannot determine the target byte order"
#endif
#define SIR_ENDIAN_NATIVE_LITTLE SIR_ENDIAN_LITTLE
#define SIR_ENDIAN_NATIVE_BIG SIR_ENDIAN_BIG

// ===========================================================================
// Filesystem, binary naming conventions and compile-target API availability.
// ===========================================================================
#if SIR_TARGET_OS_WINDOWS
#define SIR_PLATFORM_PATH_SEPARATOR '\\'
#define SIR_PLATFORM_PATH_SEPARATOR_STRING "\\"
#define SIR_PLATFORM_SHARED_LIBRARY_EXTENSION ".dll"
#define SIR_PLATFORM_EXECUTABLE_EXTENSION ".exe"
#define SIR_PLATFORM_NEWLINE "\r\n"
#elif SIR_PLATFORM_EMSCRIPTEN
#define SIR_PLATFORM_PATH_SEPARATOR '/'
#define SIR_PLATFORM_PATH_SEPARATOR_STRING "/"
#define SIR_PLATFORM_SHARED_LIBRARY_EXTENSION ".wasm"
#define SIR_PLATFORM_EXECUTABLE_EXTENSION ".wasm"
#define SIR_PLATFORM_NEWLINE "\n"
#elif SIR_PLATFORM_APPLE
#define SIR_PLATFORM_PATH_SEPARATOR '/'
#define SIR_PLATFORM_PATH_SEPARATOR_STRING "/"
#define SIR_PLATFORM_SHARED_LIBRARY_EXTENSION ".dylib"
#define SIR_PLATFORM_EXECUTABLE_EXTENSION ""
#define SIR_PLATFORM_NEWLINE "\n"
#elif SIR_TARGET_OS_LINUX || SIR_PLATFORM_ANDROID || SIR_PLATFORM_BSD
#define SIR_PLATFORM_PATH_SEPARATOR '/'
#define SIR_PLATFORM_PATH_SEPARATOR_STRING "/"
#define SIR_PLATFORM_SHARED_LIBRARY_EXTENSION ".so"
#define SIR_PLATFORM_EXECUTABLE_EXTENSION ""
#define SIR_PLATFORM_NEWLINE "\n"
#else
#error "Unsupported target platform for SIREngine path and binary conventions"
#endif

#define SIR_PLATFORM_HAS_WIN32_API SIR_TARGET_OS_WINDOWS
#define SIR_PLATFORM_HAS_POSIX_API SIR_PLATFORM_POSIX
#define SIR_PLATFORM_HAS_VIRTUAL_ALLOC SIR_TARGET_OS_WINDOWS
#define SIR_PLATFORM_HAS_MMAP SIR_PLATFORM_POSIX
#define SIR_PLATFORM_HAS_DLOPEN ( SIR_TARGET_OS_LINUX || SIR_PLATFORM_ANDROID || SIR_PLATFORM_APPLE || SIR_PLATFORM_BSD )
#define SIR_PLATFORM_HAS_IOCP SIR_TARGET_OS_WINDOWS
#define SIR_PLATFORM_HAS_EPOLL ( SIR_TARGET_OS_LINUX || SIR_PLATFORM_ANDROID )
#define SIR_PLATFORM_HAS_KQUEUE ( SIR_PLATFORM_APPLE || SIR_PLATFORM_BSD )
#define SIR_PLATFORM_HAS_INOTIFY ( SIR_TARGET_OS_LINUX || SIR_PLATFORM_ANDROID )
#define SIR_PLATFORM_HAS_FUTEX ( SIR_TARGET_OS_LINUX || SIR_PLATFORM_ANDROID )
#define SIR_PLATFORM_HAS_PTHREADS SIR_PLATFORM_POSIX
#if SIR_PLATFORM_EMSCRIPTEN && defined( __EMSCRIPTEN_PTHREADS__ )
#define SIR_PLATFORM_HAS_WEB_THREADS 1
#else
#define SIR_PLATFORM_HAS_WEB_THREADS 0
#endif
#define SIR_PLATFORM_HAS_THREADS ( SIR_TARGET_OS_WINDOWS || SIR_PLATFORM_HAS_PTHREADS || SIR_PLATFORM_HAS_WEB_THREADS )

// For builds intentionally targeting future platforms, define
// SIR_PLATFORM_REQUIRE_NATIVE_BACKEND=1 before inclusion to fail early.
#if defined( SIR_PLATFORM_REQUIRE_NATIVE_BACKEND ) && SIR_PLATFORM_REQUIRE_NATIVE_BACKEND && !SIR_PLATFORM_NATIVE_BACKEND
#error "SIREngine does not yet implement a native platform backend for this OS"
#endif

static_assert( sizeof( void * ) * 8 == SIR_PLATFORM_POINTER_BITS, "SIREngine platform pointer-width mismatch" );
static_assert( SIR_ENDIAN_LITTLE + SIR_ENDIAN_BIG == 1, "SIREngine requires an unambiguous target byte order" );

#endif // SIR_CORE_PLATFORM_PLATFORM_H
