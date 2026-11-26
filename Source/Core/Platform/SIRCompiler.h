/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#ifndef SIR_CORE_PLATFORM_COMPILER_H
#define SIR_CORE_PLATFORM_COMPILER_H

#include <Core/SIRPCH.h>

#ifndef SIR_PRAGMA_ONCE_SUPPORTED
#define SIR_PRAGMA_ONCE_SUPPORTED 1
#endif

// clang-format off
#if SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on

// ===========================================================================
// Active C++ language mode. MSVC without /Zc:__cplusplus reports 199711L in
// __cplusplus; _MSVC_LANG is the authoritative language mode there.
// ===========================================================================
#ifndef __cplusplus
#error "SIREngine headers require a C++ compiler"
#endif
#if defined( _MSVC_LANG )
#define SIR_CPP_VERSION _MSVC_LANG
#else
#define SIR_CPP_VERSION __cplusplus
#endif

#define SIR_CPP_VERSION_RAW __cplusplus
#define SIR_CPP_MINIMUM 201703L
#define SIR_CPP_IS_11 ( SIR_CPP_VERSION >= 201103L && SIR_CPP_VERSION < 201402L )
#define SIR_CPP_IS_14 ( SIR_CPP_VERSION >= 201402L && SIR_CPP_VERSION < 201703L )
#define SIR_CPP_IS_17 ( SIR_CPP_VERSION >= 201703L && SIR_CPP_VERSION < 202002L )
#define SIR_CPP_IS_20 ( SIR_CPP_VERSION >= 202002L && SIR_CPP_VERSION < 202003L )
#define SIR_CPP_IS_23 ( SIR_CPP_VERSION >= 202003L && SIR_CPP_VERSION < 202400L )
#define SIR_CPP_IS_26 ( SIR_CPP_VERSION >= 202400L )
#define SIR_CPP_AT_LEAST_17 ( SIR_CPP_VERSION >= 201703L )
#define SIR_CPP_AT_LEAST_20 ( SIR_CPP_VERSION >= 202002L )
#define SIR_CPP_AT_LEAST_23 ( SIR_CPP_VERSION >= 202003L )
#define SIR_CPP_AT_LEAST_26 ( SIR_CPP_VERSION >= 202400L )
#define SIR_CPP_VERSION_AT_LEAST( nYearMonth ) ( SIR_CPP_VERSION >= ( nYearMonth ) )

#if SIR_CPP_IS_11
#define SIR_CPP_STANDARD 11
#elif SIR_CPP_IS_14
#define SIR_CPP_STANDARD 14
#elif SIR_CPP_IS_17
#define SIR_CPP_STANDARD 17
#elif SIR_CPP_IS_20
#define SIR_CPP_STANDARD 20
#elif SIR_CPP_IS_23
#define SIR_CPP_STANDARD 23
#elif SIR_CPP_IS_26
#define SIR_CPP_STANDARD 26
#else
#define SIR_CPP_STANDARD 0
#endif

// Applications can raise the floor before including this header. Compilers
// may report a preliminary value above 202002L for their C++23 mode.
#ifndef SIR_CPP_REQUIRED_VERSION
#define SIR_CPP_REQUIRED_VERSION SIR_CPP_MINIMUM
#endif
#if SIR_CPP_VERSION < SIR_CPP_MINIMUM
#error "SIREngine requires C++17 or later"
#elif SIR_CPP_VERSION < SIR_CPP_REQUIRED_VERSION
#error "Active C++ mode is below SIR_CPP_REQUIRED_VERSION"
#endif

#if defined( _MSVC_LANG ) && defined( __cplusplus ) && _MSVC_LANG != __cplusplus
#define SIR_CPP_MSVC_LEGACY_CPLUSPLUS 1
#else
#define SIR_CPP_MSVC_LEGACY_CPLUSPLUS 0
#endif

#if defined( __STRICT_ANSI__ )
#define SIR_CPP_GNU_EXTENSIONS 0
#elif defined( __GNUC__ ) && !defined( _MSC_VER )
#define SIR_CPP_GNU_EXTENSIONS 1
#else
#define SIR_CPP_GNU_EXTENSIONS 0
#endif

// ===========================================================================
// Compiler identity, host ABI and vendor toolchain information. Frontend flags
// are exclusive; compatibility/driver flags may overlap (Clang + clang-cl etc).
// ===========================================================================
#if defined( __INTEL_LLVM_COMPILER )
#define SIR_COMPILER_INTEL_LLVM 1
#else
#define SIR_COMPILER_INTEL_LLVM 0
#endif
#if defined( __INTEL_COMPILER ) && !SIR_COMPILER_INTEL_LLVM
#define SIR_COMPILER_INTEL_CLASSIC 1
#else
#define SIR_COMPILER_INTEL_CLASSIC 0
#endif
#if defined( __clang__ )
#define SIR_COMPILER_CLANG 1
#else
#define SIR_COMPILER_CLANG 0
#endif
#if defined( _MSC_VER ) && !SIR_COMPILER_CLANG && !SIR_COMPILER_INTEL_CLASSIC
#define SIR_COMPILER_MSVC 1
#else
#define SIR_COMPILER_MSVC 0
#endif
#if defined( __GNUC__ ) && !SIR_COMPILER_CLANG && !SIR_COMPILER_INTEL_CLASSIC
#define SIR_COMPILER_GCC 1
#else
#define SIR_COMPILER_GCC 0
#endif
#if defined( __apple_build_version__ ) && SIR_COMPILER_CLANG
#define SIR_COMPILER_APPLE_CLANG 1
#else
#define SIR_COMPILER_APPLE_CLANG 0
#endif
#if SIR_COMPILER_CLANG && defined( _MSC_VER )
#define SIR_COMPILER_CLANG_CL 1
#else
#define SIR_COMPILER_CLANG_CL 0
#endif
#if defined( _MSC_VER )
#define SIR_COMPILER_MSVC_ABI 1
#else
#define SIR_COMPILER_MSVC_ABI 0
#endif
#if defined( __MINGW32__ ) || defined( __MINGW64__ )
#define SIR_COMPILER_MINGW 1
#else
#define SIR_COMPILER_MINGW 0
#endif
#if defined( __EMSCRIPTEN__ )
#define SIR_COMPILER_EMSCRIPTEN 1
#else
#define SIR_COMPILER_EMSCRIPTEN 0
#endif
#if defined( __CUDACC__ )
#define SIR_COMPILER_CUDA 1
#else
#define SIR_COMPILER_CUDA 0
#endif
#if defined( __HIPCC__ ) || defined( __HIP__ )
#define SIR_COMPILER_HIP 1
#else
#define SIR_COMPILER_HIP 0
#endif
#if defined( __CUDA_ARCH__ ) || defined( __HIP_DEVICE_COMPILE__ )
#define SIR_COMPILER_DEVICE_CODE 1
#else
#define SIR_COMPILER_DEVICE_CODE 0
#endif

// This is the actual frontend's version, not an emulated GNU/MSVC version.
// Intel LLVM exposes its own YYYYMMDD version and a Clang frontend version.
#if SIR_COMPILER_INTEL_LLVM
#define SIR_COMPILER_NAME "Intel LLVM"
#define SIR_COMPILER_VERSION_MAJOR ( __INTEL_LLVM_COMPILER / 10000 )
#define SIR_COMPILER_VERSION_MINOR ( ( __INTEL_LLVM_COMPILER / 100 ) % 100 )
#define SIR_COMPILER_VERSION_PATCH ( __INTEL_LLVM_COMPILER % 100 )
#elif SIR_COMPILER_INTEL_CLASSIC
#define SIR_COMPILER_NAME "Intel Classic"
#define SIR_COMPILER_VERSION_MAJOR ( __INTEL_COMPILER / 100 )
#define SIR_COMPILER_VERSION_MINOR ( __INTEL_COMPILER % 100 )
#if defined( __INTEL_COMPILER_UPDATE )
#define SIR_COMPILER_VERSION_PATCH __INTEL_COMPILER_UPDATE
#else
#define SIR_COMPILER_VERSION_PATCH 0
#endif
#elif SIR_COMPILER_APPLE_CLANG
#define SIR_COMPILER_NAME "Apple Clang"
#define SIR_COMPILER_VERSION_MAJOR __clang_major__
#define SIR_COMPILER_VERSION_MINOR __clang_minor__
#define SIR_COMPILER_VERSION_PATCH __clang_patchlevel__
#elif SIR_COMPILER_CLANG_CL
#define SIR_COMPILER_NAME "Clang-cl"
#define SIR_COMPILER_VERSION_MAJOR __clang_major__
#define SIR_COMPILER_VERSION_MINOR __clang_minor__
#define SIR_COMPILER_VERSION_PATCH __clang_patchlevel__
#elif SIR_COMPILER_CLANG
#define SIR_COMPILER_NAME "Clang"
#define SIR_COMPILER_VERSION_MAJOR __clang_major__
#define SIR_COMPILER_VERSION_MINOR __clang_minor__
#define SIR_COMPILER_VERSION_PATCH __clang_patchlevel__
#elif SIR_COMPILER_MSVC
#define SIR_COMPILER_NAME "MSVC"
#define SIR_COMPILER_VERSION_MAJOR ( _MSC_VER / 100 )
#define SIR_COMPILER_VERSION_MINOR ( _MSC_VER % 100 )
#define SIR_COMPILER_VERSION_PATCH 0
#elif SIR_COMPILER_GCC
#define SIR_COMPILER_NAME "GCC"
#define SIR_COMPILER_VERSION_MAJOR __GNUC__
#define SIR_COMPILER_VERSION_MINOR __GNUC_MINOR__
#define SIR_COMPILER_VERSION_PATCH __GNUC_PATCHLEVEL__
#else
#error "Unsupported compiler frontend: SIREngine needs GCC, Clang, MSVC or Intel LLVM"
#endif

#define SIR_COMPILER_MAKE_VERSION( nMajor, nMinor, nPatch ) ( ( nMajor ) * 10000 + ( nMinor ) * 100 + ( nPatch ) )
#define SIR_COMPILER_VERSION SIR_COMPILER_MAKE_VERSION( SIR_COMPILER_VERSION_MAJOR, SIR_COMPILER_VERSION_MINOR, SIR_COMPILER_VERSION_PATCH )
#define SIR_COMPILER_VERSION_AT_LEAST( nMajor, nMinor, nPatch )                                                                            \
	( SIR_COMPILER_VERSION >= SIR_COMPILER_MAKE_VERSION( nMajor, nMinor, nPatch ) )
#define SIR_COMPILER_VERSION_BEFORE( nMajor, nMinor, nPatch ) ( SIR_COMPILER_VERSION < SIR_COMPILER_MAKE_VERSION( nMajor, nMinor, nPatch ) )

// MSVC compatibility version can be queried even under clang-cl.
#if defined( _MSC_VER )
#define SIR_MSVC_TOOLSET_VERSION _MSC_VER
#if defined( _MSC_BUILD )
#define SIR_MSVC_BUILD_VERSION _MSC_BUILD
#else
#define SIR_MSVC_BUILD_VERSION 0
#endif
#if defined( _MSC_FULL_VER )
#define SIR_MSVC_FULL_VERSION _MSC_FULL_VER
#else
#define SIR_MSVC_FULL_VERSION 0
#endif
#else
#define SIR_MSVC_TOOLSET_VERSION 0
#define SIR_MSVC_BUILD_VERSION 0
#define SIR_MSVC_FULL_VERSION 0
#endif
#define SIR_MSVC_TOOLSET_AT_LEAST( nVersion ) ( SIR_MSVC_TOOLSET_VERSION >= ( nVersion ) )

// Apple Clang and upstream Clang have different release numbering systems.
#if SIR_COMPILER_APPLE_CLANG
#define SIR_APPLE_CLANG_BUILD_VERSION __apple_build_version__
#else
#define SIR_APPLE_CLANG_BUILD_VERSION 0
#endif
#if SIR_COMPILER_CLANG
#define SIR_CLANG_VERSION SIR_COMPILER_MAKE_VERSION( __clang_major__, __clang_minor__, __clang_patchlevel__ )
#else
#define SIR_CLANG_VERSION 0
#endif
#if defined( __GNUC__ ) && !SIR_COMPILER_CLANG && !SIR_COMPILER_INTEL_CLASSIC
#define SIR_GCC_VERSION SIR_COMPILER_MAKE_VERSION( __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__ )
#else
#define SIR_GCC_VERSION 0
#endif
#if SIR_COMPILER_INTEL_LLVM
#define SIR_INTEL_LLVM_VERSION __INTEL_LLVM_COMPILER
#else
#define SIR_INTEL_LLVM_VERSION 0
#endif

// Conservative frontend floors. Override before inclusion for controlled
// legacy experiments; a C++17 language check remains mandatory independently.
#ifndef SIR_MIN_GCC_VERSION
#define SIR_MIN_GCC_VERSION SIR_COMPILER_MAKE_VERSION( 9, 0, 0 )
#endif
#ifndef SIR_MIN_CLANG_VERSION
#define SIR_MIN_CLANG_VERSION SIR_COMPILER_MAKE_VERSION( 10, 0, 0 )
#endif
#ifndef SIR_MIN_APPLE_CLANG_VERSION
#define SIR_MIN_APPLE_CLANG_VERSION SIR_COMPILER_MAKE_VERSION( 10, 0, 0 )
#endif
#ifndef SIR_MIN_MSVC_TOOLSET_VERSION
#define SIR_MIN_MSVC_TOOLSET_VERSION 1916
#endif
#ifndef SIR_MIN_INTEL_CLASSIC_VERSION
#define SIR_MIN_INTEL_CLASSIC_VERSION SIR_COMPILER_MAKE_VERSION( 19, 0, 0 )
#endif

#if SIR_COMPILER_INTEL_CLASSIC && SIR_COMPILER_VERSION < SIR_MIN_INTEL_CLASSIC_VERSION
#error "SIREngine Intel Classic version is below SIR_MIN_INTEL_CLASSIC_VERSION"
#elif SIR_COMPILER_APPLE_CLANG && SIR_COMPILER_VERSION < SIR_MIN_APPLE_CLANG_VERSION
#error "SIREngine Apple Clang version is below SIR_MIN_APPLE_CLANG_VERSION"
#elif SIR_COMPILER_CLANG && !SIR_COMPILER_INTEL_LLVM && !SIR_COMPILER_APPLE_CLANG && SIR_CLANG_VERSION < SIR_MIN_CLANG_VERSION
#error "SIREngine Clang version is below SIR_MIN_CLANG_VERSION"
#elif SIR_COMPILER_MSVC_ABI && SIR_MSVC_TOOLSET_VERSION < SIR_MIN_MSVC_TOOLSET_VERSION
#error "SIREngine MSVC toolset is below SIR_MIN_MSVC_TOOLSET_VERSION"
#elif SIR_COMPILER_GCC && SIR_GCC_VERSION < SIR_MIN_GCC_VERSION
#error "SIREngine GCC version is below SIR_MIN_GCC_VERSION"
#endif

// ===========================================================================
// Compile-time probes: these are suitable for use inside #if expressions.
// Prefer feature checks over inferring C++ feature completeness from version.
// ===========================================================================
#if defined( __has_builtin )
#define SIR_COMPILER_HAS_BUILTIN( name ) __has_builtin( name )
#else
#define SIR_COMPILER_HAS_BUILTIN( name ) 0
#endif
#if defined( __has_constexpr_builtin )
#define SIR_COMPILER_HAS_CONSTEXPR_BUILTIN( name ) __has_constexpr_builtin( name )
#else
#define SIR_COMPILER_HAS_CONSTEXPR_BUILTIN( name ) 0
#endif
#if defined( __has_attribute )
#define SIR_COMPILER_HAS_ATTRIBUTE( name ) __has_attribute( name )
#else
#define SIR_COMPILER_HAS_ATTRIBUTE( name ) 0
#endif
#if defined( __has_declspec_attribute )
#define SIR_COMPILER_HAS_DECLSPEC_ATTRIBUTE( name ) __has_declspec_attribute( name )
#else
#define SIR_COMPILER_HAS_DECLSPEC_ATTRIBUTE( name ) 0
#endif
#if defined( __has_cpp_attribute )
#define SIR_COMPILER_HAS_CPP_ATTRIBUTE( name ) __has_cpp_attribute( name )
#else
#define SIR_COMPILER_HAS_CPP_ATTRIBUTE( name ) 0
#endif
#if defined( __has_feature )
#define SIR_COMPILER_HAS_FEATURE( name ) __has_feature( name )
#else
#define SIR_COMPILER_HAS_FEATURE( name ) 0
#endif
#if defined( __has_extension )
#define SIR_COMPILER_HAS_EXTENSION( name ) __has_extension( name )
#else
#define SIR_COMPILER_HAS_EXTENSION( name ) 0
#endif
#if defined( __has_include )
#define SIR_COMPILER_HAS_INCLUDE( header ) __has_include( header )
#else
#define SIR_COMPILER_HAS_INCLUDE( header ) 0
#endif
#if defined( __has_warning )
#define SIR_COMPILER_HAS_WARNING( name ) __has_warning( name )
#else
#define SIR_COMPILER_HAS_WARNING( name ) 0
#endif

// Core feature-test macros do not require including standard library headers.
// The gates check real implementation support, not solely the language mode.
#if defined( __cpp_if_constexpr ) && __cpp_if_constexpr >= 201606L
#define SIR_CPP_HAS_IF_CONSTEXPR 1
#else
#define SIR_CPP_HAS_IF_CONSTEXPR 0
#endif
#if defined( __cpp_fold_expressions ) && __cpp_fold_expressions >= 201603L
#define SIR_CPP_HAS_FOLD_EXPRESSIONS 1
#else
#define SIR_CPP_HAS_FOLD_EXPRESSIONS 0
#endif
#if defined( __cpp_structured_bindings ) && __cpp_structured_bindings >= 201606L
#define SIR_CPP_HAS_STRUCTURED_BINDINGS 1
#else
#define SIR_CPP_HAS_STRUCTURED_BINDINGS 0
#endif
#if defined( __cpp_inline_variables ) && __cpp_inline_variables >= 201606L
#define SIR_CPP_HAS_INLINE_VARIABLES 1
#else
#define SIR_CPP_HAS_INLINE_VARIABLES 0
#endif
#if defined( __cpp_guaranteed_copy_elision ) && __cpp_guaranteed_copy_elision >= 201606L
#define SIR_CPP_HAS_GUARANTEED_COPY_ELISION 1
#else
#define SIR_CPP_HAS_GUARANTEED_COPY_ELISION 0
#endif
#if defined( __cpp_aligned_new ) && __cpp_aligned_new >= 201606L
#define SIR_CPP_HAS_ALIGNED_NEW 1
#else
#define SIR_CPP_HAS_ALIGNED_NEW 0
#endif
#if defined( __cpp_nontype_template_parameter_auto ) && __cpp_nontype_template_parameter_auto >= 201606L
#define SIR_CPP_HAS_NONTYPE_TEMPLATE_AUTO 1
#else
#define SIR_CPP_HAS_NONTYPE_TEMPLATE_AUTO 0
#endif
#if defined( __cpp_constexpr ) && __cpp_constexpr >= 201603L
#define SIR_CPP_HAS_CONSTEXPR_LAMBDA 1
#else
#define SIR_CPP_HAS_CONSTEXPR_LAMBDA 0
#endif
#if defined( __cpp_impl_three_way_comparison ) && __cpp_impl_three_way_comparison >= 201907L
#define SIR_CPP_HAS_THREE_WAY_COMPARISON 1
#else
#define SIR_CPP_HAS_THREE_WAY_COMPARISON 0
#endif
#if defined( __cpp_concepts ) && __cpp_concepts >= 201907L
#define SIR_CPP_HAS_CONCEPTS 1
#else
#define SIR_CPP_HAS_CONCEPTS 0
#endif
#if defined( __cpp_consteval ) && __cpp_consteval >= 201811L
#define SIR_CPP_HAS_CONSTEVAL 1
#else
#define SIR_CPP_HAS_CONSTEVAL 0
#endif
#if defined( __cpp_constinit ) && __cpp_constinit >= 201907L
#define SIR_CPP_HAS_CONSTINIT 1
#else
#define SIR_CPP_HAS_CONSTINIT 0
#endif
#if defined( __cpp_impl_coroutine ) && __cpp_impl_coroutine >= 201902L
#define SIR_CPP_HAS_COROUTINES 1
#else
#define SIR_CPP_HAS_COROUTINES 0
#endif
#if defined( __cpp_designated_initializers ) && __cpp_designated_initializers >= 201707L
#define SIR_CPP_HAS_DESIGNATED_INITIALIZERS 1
#else
#define SIR_CPP_HAS_DESIGNATED_INITIALIZERS 0
#endif
#if defined( __cpp_nontype_template_args ) && __cpp_nontype_template_args >= 201911L
#define SIR_CPP_HAS_NON_TYPE_TEMPLATE_CLASS 1
#else
#define SIR_CPP_HAS_NON_TYPE_TEMPLATE_CLASS 0
#endif
#if defined( __cpp_constexpr_dynamic_alloc ) && __cpp_constexpr_dynamic_alloc >= 201907L
#define SIR_CPP_HAS_CONSTEXPR_DYNAMIC_ALLOC 1
#else
#define SIR_CPP_HAS_CONSTEXPR_DYNAMIC_ALLOC 0
#endif
#if defined( __cpp_char8_t ) && __cpp_char8_t >= 201811L
#define SIR_CPP_HAS_CHAR8_T 1
#else
#define SIR_CPP_HAS_CHAR8_T 0
#endif
#if defined( __cpp_modules ) && __cpp_modules >= 201907L
#define SIR_CPP_HAS_MODULES 1
#else
#define SIR_CPP_HAS_MODULES 0
#endif
#if defined( __cpp_if_consteval ) && __cpp_if_consteval >= 202106L
#define SIR_CPP_HAS_IF_CONSTEVAL 1
#else
#define SIR_CPP_HAS_IF_CONSTEVAL 0
#endif
#if defined( __cpp_explicit_this_parameter ) && __cpp_explicit_this_parameter >= 202110L
#define SIR_CPP_HAS_EXPLICIT_OBJECT_PARAMETER 1
#else
#define SIR_CPP_HAS_EXPLICIT_OBJECT_PARAMETER 0
#endif
#if defined( __cpp_multidimensional_subscript ) && __cpp_multidimensional_subscript >= 202110L
#define SIR_CPP_HAS_MULTIDIM_SUBSCRIPT 1
#else
#define SIR_CPP_HAS_MULTIDIM_SUBSCRIPT 0
#endif
#if defined( __cpp_static_call_operator ) && __cpp_static_call_operator >= 202207L
#define SIR_CPP_HAS_STATIC_CALL_OPERATOR 1
#else
#define SIR_CPP_HAS_STATIC_CALL_OPERATOR 0
#endif
#if defined( __cpp_size_t_suffix ) && __cpp_size_t_suffix >= 202011L
#define SIR_CPP_HAS_SIZE_T_SUFFIX 1
#else
#define SIR_CPP_HAS_SIZE_T_SUFFIX 0
#endif
#if defined( __cpp_auto_cast ) && __cpp_auto_cast >= 202110L
#define SIR_CPP_HAS_AUTO_CAST 1
#else
#define SIR_CPP_HAS_AUTO_CAST 0
#endif
#if defined( __cpp_pack_indexing ) && __cpp_pack_indexing >= 202311L
#define SIR_CPP_HAS_PACK_INDEXING 1
#else
#define SIR_CPP_HAS_PACK_INDEXING 0
#endif
#if defined( __cpp_static_assert ) && __cpp_static_assert >= 202306L
#define SIR_CPP_HAS_STATIC_ASSERT_2023 1
#else
#define SIR_CPP_HAS_STATIC_ASSERT_2023 0
#endif

// <version> is the standard-library's feature-test table. Gate loading so
// freestanding toolchains and reduced SDKs still compile this header.
#ifndef SIR_CPP_PROBE_STDLIB
#define SIR_CPP_PROBE_STDLIB 1
#endif
#if SIR_CPP_PROBE_STDLIB && SIR_COMPILER_HAS_INCLUDE( <version> )
#include <version>
#define SIR_CPP_HAS_LIBRARY_VERSION_HEADER 1
#else
#define SIR_CPP_HAS_LIBRARY_VERSION_HEADER 0
#endif

#if defined( __cpp_lib_filesystem ) && __cpp_lib_filesystem >= 201703L
#define SIR_CPP_LIB_FILESYSTEM 1
#else
#define SIR_CPP_LIB_FILESYSTEM 0
#endif
#if defined( __cpp_lib_string_view ) && __cpp_lib_string_view >= 201606L
#define SIR_CPP_LIB_STRING_VIEW 1
#else
#define SIR_CPP_LIB_STRING_VIEW 0
#endif
#if defined( __cpp_lib_optional ) && __cpp_lib_optional >= 201606L
#define SIR_CPP_LIB_OPTIONAL 1
#else
#define SIR_CPP_LIB_OPTIONAL 0
#endif
#if defined( __cpp_lib_variant ) && __cpp_lib_variant >= 201606L
#define SIR_CPP_LIB_VARIANT 1
#else
#define SIR_CPP_LIB_VARIANT 0
#endif
#if defined( __cpp_lib_parallel_algorithm ) && __cpp_lib_parallel_algorithm >= 201603L
#define SIR_CPP_LIB_PARALLEL_ALGORITHM 1
#else
#define SIR_CPP_LIB_PARALLEL_ALGORITHM 0
#endif
#if defined( __cpp_lib_span ) && __cpp_lib_span >= 202002L
#define SIR_CPP_LIB_SPAN 1
#else
#define SIR_CPP_LIB_SPAN 0
#endif
#if defined( __cpp_lib_ranges ) && __cpp_lib_ranges >= 201911L
#define SIR_CPP_LIB_RANGES 1
#else
#define SIR_CPP_LIB_RANGES 0
#endif
#if defined( __cpp_lib_concepts ) && __cpp_lib_concepts >= 202002L
#define SIR_CPP_LIB_CONCEPTS 1
#else
#define SIR_CPP_LIB_CONCEPTS 0
#endif
#if defined( __cpp_lib_atomic_ref ) && __cpp_lib_atomic_ref >= 201806L
#define SIR_CPP_LIB_ATOMIC_REF 1
#else
#define SIR_CPP_LIB_ATOMIC_REF 0
#endif
#if defined( __cpp_lib_jthread ) && __cpp_lib_jthread >= 201911L
#define SIR_CPP_LIB_JTHREAD 1
#else
#define SIR_CPP_LIB_JTHREAD 0
#endif
#if defined( __cpp_lib_bit_cast ) && __cpp_lib_bit_cast >= 201806L
#define SIR_CPP_LIB_BIT_CAST 1
#else
#define SIR_CPP_LIB_BIT_CAST 0
#endif
#if defined( __cpp_lib_endian ) && __cpp_lib_endian >= 201907L
#define SIR_CPP_LIB_ENDIAN 1
#else
#define SIR_CPP_LIB_ENDIAN 0
#endif
#if defined( __cpp_lib_format ) && __cpp_lib_format >= 201907L
#define SIR_CPP_LIB_FORMAT 1
#else
#define SIR_CPP_LIB_FORMAT 0
#endif
#if defined( __cpp_lib_source_location ) && __cpp_lib_source_location >= 201907L
#define SIR_CPP_LIB_SOURCE_LOCATION 1
#else
#define SIR_CPP_LIB_SOURCE_LOCATION 0
#endif
#if defined( __cpp_lib_expected ) && __cpp_lib_expected >= 202202L
#define SIR_CPP_LIB_EXPECTED 1
#else
#define SIR_CPP_LIB_EXPECTED 0
#endif
#if defined( __cpp_lib_stacktrace ) && __cpp_lib_stacktrace >= 202011L
#define SIR_CPP_LIB_STACKTRACE 1
#else
#define SIR_CPP_LIB_STACKTRACE 0
#endif
#if defined( __cpp_lib_print ) && __cpp_lib_print >= 202207L
#define SIR_CPP_LIB_PRINT 1
#else
#define SIR_CPP_LIB_PRINT 0
#endif
#if defined( __cpp_lib_mdspan ) && __cpp_lib_mdspan >= 202207L
#define SIR_CPP_LIB_MDSPAN 1
#else
#define SIR_CPP_LIB_MDSPAN 0
#endif
#if defined( __cpp_lib_flat_map ) && __cpp_lib_flat_map >= 202207L
#define SIR_CPP_LIB_FLAT_MAP 1
#else
#define SIR_CPP_LIB_FLAT_MAP 0
#endif
#if defined( __cpp_lib_byteswap ) && __cpp_lib_byteswap >= 202110L
#define SIR_CPP_LIB_BYTESWAP 1
#else
#define SIR_CPP_LIB_BYTESWAP 0
#endif
#if defined( __cpp_lib_to_underlying ) && __cpp_lib_to_underlying >= 202102L
#define SIR_CPP_LIB_TO_UNDERLYING 1
#else
#define SIR_CPP_LIB_TO_UNDERLYING 0
#endif
#if defined( __cpp_lib_simd ) && __cpp_lib_simd >= 202411L
#define SIR_CPP_LIB_SIMD 1
#else
#define SIR_CPP_LIB_SIMD 0
#endif

// The C++ library is independently versioned from its compiler frontend.
// Detection is evidence-based: unknown means the library did not identify
// itself through a header, not that a given vendor is absent from the SDK.
#if defined( _LIBCPP_VERSION )
#define SIR_STDLIB_LIBCXX 1
#define SIR_STDLIB_LIBSTDCXX 0
#define SIR_STDLIB_MSVC 0
#define SIR_STDLIB_NAME "libc++"
#define SIR_STDLIB_VERSION _LIBCPP_VERSION
#elif defined( __GLIBCXX__ )
#define SIR_STDLIB_LIBCXX 0
#define SIR_STDLIB_LIBSTDCXX 1
#define SIR_STDLIB_MSVC 0
#define SIR_STDLIB_NAME "libstdc++"
#if defined( _GLIBCXX_RELEASE )
#define SIR_STDLIB_VERSION _GLIBCXX_RELEASE
#else
#define SIR_STDLIB_VERSION __GLIBCXX__
#endif
#elif defined( _MSVC_STL_VERSION )
#define SIR_STDLIB_LIBCXX 0
#define SIR_STDLIB_LIBSTDCXX 0
#define SIR_STDLIB_MSVC 1
#define SIR_STDLIB_NAME "MSVC STL"
#define SIR_STDLIB_VERSION _MSVC_STL_VERSION
#else
#define SIR_STDLIB_LIBCXX 0
#define SIR_STDLIB_LIBSTDCXX 0
#define SIR_STDLIB_MSVC 0
#define SIR_STDLIB_NAME "Unknown"
#define SIR_STDLIB_VERSION 0
#endif
#define SIR_STDLIB_UNKNOWN ( !SIR_STDLIB_LIBCXX && !SIR_STDLIB_LIBSTDCXX && !SIR_STDLIB_MSVC )

#define SIR_COMPILER_STRINGIFY_IMPL( value ) #value
#define SIR_COMPILER_STRINGIFY( value ) SIR_COMPILER_STRINGIFY_IMPL( value )
#define SIR_CPP_STANDARD_STRING SIR_COMPILER_STRINGIFY( SIR_CPP_STANDARD )
#if defined( __VERSION__ )
#define SIR_COMPILER_VERSION_STRING __VERSION__
#elif defined( _MSC_FULL_VER )
#define SIR_COMPILER_VERSION_STRING SIR_COMPILER_STRINGIFY( _MSC_FULL_VER )
#else
#define SIR_COMPILER_VERSION_STRING "Unknown"
#endif

// ===========================================================================
// Runtime/build configuration and sanitizer inspection.
// ===========================================================================
#if defined( __cpp_exceptions ) || defined( __EXCEPTIONS ) || defined( _CPPUNWIND )
#define SIR_COMPILER_EXCEPTIONS 1
#else
#define SIR_COMPILER_EXCEPTIONS 0
#endif
#if defined( __cpp_rtti ) || defined( __GXX_RTTI ) || defined( _CPPRTTI )
#define SIR_COMPILER_RTTI 1
#else
#define SIR_COMPILER_RTTI 0
#endif
#if defined( __SANITIZE_ADDRESS__ ) || SIR_COMPILER_HAS_FEATURE( address_sanitizer )
#define SIR_COMPILER_ASAN 1
#else
#define SIR_COMPILER_ASAN 0
#endif
#if defined( __SANITIZE_THREAD__ ) || SIR_COMPILER_HAS_FEATURE( thread_sanitizer )
#define SIR_COMPILER_TSAN 1
#else
#define SIR_COMPILER_TSAN 0
#endif
#if defined( __SANITIZE_UNDEFINED__ ) || SIR_COMPILER_HAS_FEATURE( undefined_behavior_sanitizer )
#define SIR_COMPILER_UBSAN 1
#else
#define SIR_COMPILER_UBSAN 0
#endif
#if defined( __SANITIZE_MEMORY__ ) || SIR_COMPILER_HAS_FEATURE( memory_sanitizer )
#define SIR_COMPILER_MSAN 1
#else
#define SIR_COMPILER_MSAN 0
#endif
#if defined( __OPTIMIZE__ )
#define SIR_COMPILER_OPTIMIZED 1
#define SIR_COMPILER_OPTIMIZATION_KNOWN 1
#elif SIR_COMPILER_MSVC_ABI
// MSVC-ABI frontends have no reliable universal /O2 or /Od macro.
#define SIR_COMPILER_OPTIMIZED 0
#define SIR_COMPILER_OPTIMIZATION_KNOWN 0
#else
#define SIR_COMPILER_OPTIMIZED 0
#define SIR_COMPILER_OPTIMIZATION_KNOWN 1
#endif
#if defined( __LTO__ )
#define SIR_COMPILER_LTO 1
#else
#define SIR_COMPILER_LTO 0
#endif
#if defined( __PIC__ ) || defined( __pic__ )
#define SIR_COMPILER_PIC 1
#else
#define SIR_COMPILER_PIC 0
#endif

// ===========================================================================
// Attributes, optimization, ABI and diagnostics. Forcible function inlining
// must use SIR_FORCEINLINE; there is no ordinary inline macro.
// ===========================================================================
#if SIR_COMPILER_MSVC_ABI
#define SIR_FORCEINLINE __forceinline
#define SIR_NOINLINE __declspec( noinline )
#define SIR_RESTRICT __restrict
#define SIR_ALIGN_DECL( nAlignment ) __declspec( align( nAlignment ) )
#else
#define SIR_FORCEINLINE inline __attribute__( ( always_inline ) )
#define SIR_NOINLINE __attribute__( ( noinline ) )
#define SIR_RESTRICT __restrict__
#define SIR_ALIGN_DECL( nAlignment ) __attribute__( ( aligned( nAlignment ) ) )
#endif

#define SIR_NODISCARD [[nodiscard]]
#define SIR_NORETURN [[noreturn]]
#define SIR_MAYBE_UNUSED [[maybe_unused]]
#define SIR_FALLTHROUGH [[fallthrough]]
#define SIR_DEPRECATED( szMessage ) [[deprecated( szMessage )]]
#define SIR_ALIGNAS( nAlignment ) alignas( nAlignment )
#define SIR_ALIGNOF( type ) alignof( type )
#define SIR_THREAD_LOCAL thread_local
#define SIR_FUNCTION_NAME __func__

#if SIR_CPP_AT_LEAST_20 && SIR_COMPILER_HAS_CPP_ATTRIBUTE( nodiscard ) >= 201907
#define SIR_NODISCARD_REASON( szReason ) [[nodiscard( szReason )]]
#else
#define SIR_NODISCARD_REASON( szReason ) [[nodiscard]]
#endif
#if SIR_CPP_AT_LEAST_20 && SIR_COMPILER_HAS_CPP_ATTRIBUTE( no_unique_address ) >= 201803
#define SIR_NO_UNIQUE_ADDRESS [[no_unique_address]]
#elif SIR_CPP_AT_LEAST_20 && SIR_COMPILER_HAS_CPP_ATTRIBUTE( msvc::no_unique_address )
#define SIR_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#else
#define SIR_NO_UNIQUE_ADDRESS
#endif
#if SIR_CPP_AT_LEAST_20 && SIR_COMPILER_HAS_CPP_ATTRIBUTE( likely ) >= 201803
#define SIR_IF_LIKELY [[likely]]
#define SIR_IF_UNLIKELY [[unlikely]]
#else
#define SIR_IF_LIKELY
#define SIR_IF_UNLIKELY
#endif

#if SIR_COMPILER_MSVC_ABI
#define SIR_FUNCTION_SIGNATURE __FUNCSIG__
#else
#define SIR_FUNCTION_SIGNATURE __PRETTY_FUNCTION__
#endif

#if SIR_COMPILER_HAS_BUILTIN( __builtin_expect ) || SIR_COMPILER_GCC || SIR_COMPILER_CLANG
#define SIR_LIKELY( expression ) __builtin_expect( !!( expression ), 1 )
#define SIR_UNLIKELY( expression ) __builtin_expect( !!( expression ), 0 )
#else
#define SIR_LIKELY( expression ) ( expression )
#define SIR_UNLIKELY( expression ) ( expression )
#endif

// Promises, not checks. Passing a false condition causes undefined behavior.
#if SIR_COMPILER_MSVC_ABI
#define SIR_COMPILER_UNREACHABLE() __assume( 0 )
#define SIR_ASSUME( expression ) __assume( expression )
#elif SIR_COMPILER_HAS_BUILTIN( __builtin_assume )
#define SIR_COMPILER_UNREACHABLE() __builtin_unreachable()
#define SIR_ASSUME( expression ) __builtin_assume( expression )
#else
#define SIR_COMPILER_UNREACHABLE() __builtin_unreachable()
#define SIR_ASSUME( expression )                                                                                                           \
	do {                                                                                                                                   \
		if ( !( expression ) ) {                                                                                                           \
			SIR_COMPILER_UNREACHABLE();                                                                                                     \
		}                                                                                                                                  \
	} while ( 0 )
#endif

#if SIR_COMPILER_MSVC_ABI
#define SIR_DEBUG_BREAK() __debugbreak()
#elif SIR_COMPILER_HAS_BUILTIN( __builtin_debugtrap )
#define SIR_DEBUG_BREAK() __builtin_debugtrap()
#elif SIR_COMPILER_HAS_BUILTIN( __builtin_trap ) || SIR_COMPILER_GCC
#define SIR_DEBUG_BREAK() __builtin_trap()
#else
#define SIR_DEBUG_BREAK() ( ( void )0 )
#endif

#if defined( _WIN32 )
#define SIR_EXPORT __declspec( dllexport )
#define SIR_IMPORT __declspec( dllimport )
#define SIR_HIDDEN
#elif SIR_COMPILER_CLANG || SIR_COMPILER_GCC
#define SIR_EXPORT __attribute__( ( visibility( "default" ) ) )
#define SIR_IMPORT __attribute__( ( visibility( "default" ) ) )
#define SIR_HIDDEN __attribute__( ( visibility( "hidden" ) ) )
#else
#define SIR_EXPORT
#define SIR_IMPORT
#define SIR_HIDDEN
#endif

#if defined( SIR_BUILD_SHARED ) && SIR_BUILD_SHARED
#if defined( SIR_BUILDING_SIRENGINE ) && SIR_BUILDING_SIRENGINE
#define SIR_API SIR_EXPORT
#else
#define SIR_API SIR_IMPORT
#endif
#else
#define SIR_API
#endif

#define SIR_EXTERN_C extern "C"
#define SIR_EXTERN_C_BEGIN extern "C" {
#define SIR_EXTERN_C_END }

#if defined( _WIN32 ) && ( defined( _M_IX86 ) || defined( __i386__ ) )
#if SIR_COMPILER_MSVC_ABI
#define SIR_CDECL __cdecl
#define SIR_STDCALL __stdcall
#define SIR_FASTCALL __fastcall
#else
#define SIR_CDECL __attribute__( ( cdecl ) )
#define SIR_STDCALL __attribute__( ( stdcall ) )
#define SIR_FASTCALL __attribute__( ( fastcall ) )
#endif
#else
#define SIR_CDECL
#define SIR_STDCALL
#define SIR_FASTCALL
#endif

#if SIR_COMPILER_MSVC_ABI
#define SIR_DIAGNOSTIC_PUSH __pragma( warning( push ) )
#define SIR_DIAGNOSTIC_POP __pragma( warning( pop ) )
#define SIR_DIAGNOSTIC_IGNORE( nWarning ) __pragma( warning( disable : nWarning ) )
#define SIR_PACK_PUSH( nAlignment ) __pragma( pack( push, nAlignment ) )
#define SIR_PACK_POP() __pragma( pack( pop ) )
#else
#define SIR_COMPILER_DO_PRAGMA( expression ) _Pragma( #expression )
#define SIR_DIAGNOSTIC_PUSH SIR_COMPILER_DO_PRAGMA( GCC diagnostic push )
#define SIR_DIAGNOSTIC_POP SIR_COMPILER_DO_PRAGMA( GCC diagnostic pop )
#define SIR_DIAGNOSTIC_IGNORE( szWarning ) SIR_COMPILER_DO_PRAGMA( GCC diagnostic ignored szWarning )
#define SIR_PACK_PUSH( nAlignment ) SIR_COMPILER_DO_PRAGMA( pack( push, nAlignment ) )
#define SIR_PACK_POP() SIR_COMPILER_DO_PRAGMA( pack( pop ) )
#endif

#endif // SIR_CORE_PLATFORM_COMPILER_H
