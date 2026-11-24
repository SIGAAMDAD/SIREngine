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

#ifndef __SIR_PCH_H__
#define __SIR_PCH_H__

// clang-format off
#if defined(SIR_PRAGMA_ONCE_SUPPORTED) && SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on

// The unified PCH is a compilation foundation, not a dependency on any
// renderer, operating-system SDK, third-party library, or engine subsystem.
// Deliberately include the entire applicable standard-library header surface.
// Including a standard header does not authorize its API for use by engine code.
#ifndef __cplusplus
#error "SIREngine requires a C++ compiler"
#endif

#if defined( _MSVC_LANG )
#define SIR_PCH_CPP_VERSION _MSVC_LANG
#else
#define SIR_PCH_CPP_VERSION __cplusplus
#endif

#if SIR_PCH_CPP_VERSION < 201703L
#error "SIREngine requires C++17 or later"
#endif

// Legacy C global names remain visible for existing native source.
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fenv.h>
#include <float.h>
#include <inttypes.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <uchar.h>
#include <wchar.h>
#include <wctype.h>

// Standard C++17 headers, including the full container, concurrency,
// memory, filesystem, I/O, locales, numerics, and algorithms libraries.
#include <algorithm>
#include <any>
#include <array>
#include <atomic>
#include <bitset>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cfenv>
#include <cfloat>
#include <charconv>
#include <chrono>
#include <cinttypes>
#include <climits>
#include <clocale>
#include <cmath>
#include <complex>
#include <condition_variable>
#include <csetjmp>
#include <csignal>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cuchar>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <cwctype>
#include <deque>
#include <exception>
#include <execution>
#include <filesystem>
#include <forward_list>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <initializer_list>
#include <ios>
#include <iosfwd>
#include <iostream>
#include <istream>
#include <iterator>
#include <limits>
#include <list>
#include <locale>
#include <map>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <new>
#include <numeric>
#include <optional>
#include <ostream>
#include <queue>
#include <random>
#include <ratio>
#include <regex>
#include <scoped_allocator>
#include <set>
#include <shared_mutex>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <string_view>
// <strstream> is part of C++17 but some libraries warn on inclusion.
#if defined( __GNUC__ ) || defined( __clang__ )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcpp"
#endif
#if defined( __has_include )
#if __has_include( <strstream>)
#include <strstream>
#endif
#else
#include <strstream>
#endif
#if defined( __GNUC__ ) || defined( __clang__ )
#pragma GCC diagnostic pop
#endif
#include <system_error>
#include <thread>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <valarray>
#include <variant>
#include <vector>

// Deprecated compatibility headers still defined by older standards.
#if !defined( __has_include ) || __has_include( <codecvt>)
#include <codecvt>
#endif
#if SIR_PCH_CPP_VERSION < 202002L
#if !defined( __has_include ) || __has_include( <ciso646>)
#include <ciso646>
#endif
#if !defined( __has_include ) || __has_include( <cstdalign>)
#include <cstdalign>
#endif
#if !defined( __has_include ) || __has_include( <cstdbool>)
#include <cstdbool>
#endif
#endif

// C compatibility extensions are not present on every standard library.
// Include only when the actual implementation provides them.
#if defined( __has_include )
#if __has_include( <iso646.h>)
#include <iso646.h>
#endif
#if __has_include( <stdalign.h>)
#include <stdalign.h>
#endif
#if __has_include( <stdbool.h>)
#include <stdbool.h>
#endif
#endif

// C++20: these headers require a C++20 language mode and library support.
#if SIR_PCH_CPP_VERSION >= 202002L
#if defined( __has_include ) && __has_include( <barrier>)
#include <barrier>
#endif
#if defined( __has_include ) && __has_include( <bit>)
#include <bit>
#endif
#if defined( __has_include ) && __has_include( <compare>)
#include <compare>
#endif
#if defined( __has_include ) && __has_include( <concepts>)
#include <concepts>
#endif
#if defined( __has_include ) && __has_include( <coroutine>)
#include <coroutine>
#endif
#if defined( __has_include ) && __has_include( <format>)
#include <format>
#endif
#if defined( __has_include ) && __has_include( <latch>)
#include <latch>
#endif
#if defined( __has_include ) && __has_include( <numbers>)
#include <numbers>
#endif
#if defined( __has_include ) && __has_include( <ranges>)
#include <ranges>
#endif
#if defined( __has_include ) && __has_include( <semaphore>)
#include <semaphore>
#endif
#if defined( __has_include ) && __has_include( <source_location>)
#include <source_location>
#endif
#if defined( __has_include ) && __has_include( <span>)
#include <span>
#endif
#if defined( __has_include ) && __has_include( <stop_token>)
#include <stop_token>
#endif
#if defined( __has_include ) && __has_include( <syncstream>)
#include <syncstream>
#endif
#if defined( __has_include ) && __has_include( <version>)
#include <version>
#endif
#endif

// C++23: individual implementations may ship only some of these headers.
#if SIR_PCH_CPP_VERSION >= 202302L
#if defined( __has_include ) && __has_include( <expected>)
#include <expected>
#endif
#if defined( __has_include ) && __has_include( <flat_map>)
#include <flat_map>
#endif
#if defined( __has_include ) && __has_include( <flat_set>)
#include <flat_set>
#endif
#if defined( __has_include ) && __has_include( <generator>)
#include <generator>
#endif
#if defined( __has_include ) && __has_include( <mdspan>)
#include <mdspan>
#endif
#if defined( __has_include ) && __has_include( <print>)
#include <print>
#endif
#if defined( __has_include ) && __has_include( <spanstream>)
#include <spanstream>
#endif
#if defined( __has_include ) && __has_include( <stacktrace>)
#include <stacktrace>
#endif
#if defined( __has_include ) && __has_include( <stdfloat>)
#include <stdfloat>
#endif
#if defined( __has_include ) && __has_include( <stdatomic.h>)
#include <stdatomic.h>
#endif
#endif

// C++26 and implementation preview headers. A header being present does not
// imply complete feature support; use SIRCompiler feature probes at call sites.
#if SIR_PCH_CPP_VERSION >= 202400L
#if defined( __has_include ) && __has_include( <debugging>)
#include <debugging>
#endif
#if defined( __has_include ) && __has_include( <hazard_pointer>)
#include <hazard_pointer>
#endif
#if defined( __has_include ) && __has_include( <hive>)
#include <hive>
#endif
#if defined( __has_include ) && __has_include( <inplace_vector>)
#include <inplace_vector>
#endif
#if defined( __has_include ) && __has_include( <linalg>)
#include <linalg>
#endif
#if defined( __has_include ) && __has_include( <rcu>)
#include <rcu>
#endif
#if defined( __has_include ) && __has_include( <simd>)
#include <simd>
#endif
#if defined( __has_include ) && __has_include( <text_encoding>)
#include <text_encoding>
#endif
#endif

// Standard-library feature-test macros for C++17+ consumers.
#if defined( __has_include ) && __has_include( <version>)
#include <version>
#endif

// Keep compiler/platform reflection independent of standard-library ordering.
// Their current implementations include this PCH, so avoid re-entering a
// partially-defined companion header while the PCH is under construction.
#if !defined( SIR_CORE_PLATFORM_COMPILER_H ) && !defined( SIR_CORE_PLATFORM_PLATFORM_H )
#include <Core/Platform/SIRCompiler.h>
#include <Core/Platform/SIRPlatform.h>
#endif

#endif // __SIR_PCH_H__
