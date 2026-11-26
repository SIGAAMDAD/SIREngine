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

#ifndef __SIR_ASSERT_H__
#define __SIR_ASSERT_H__

#if defined( SIR_PRAGMA_ONCE_SUPPORTED ) && SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Core/SIRPCH.h>

#include <Core/Diagnostics/Assertions/SIRAssertTypes.h>
#include <Core/Diagnostics/SIRDiagnosticBuildPolicy.h>
#include <Core/Platform/SIRCompiler.h>

// SIR_UNREACHABLE is a diagnostic contract, not an optimizer promise. The
// compiler header's optimizer-only spelling is intentionally replaced here.
#ifdef SIR_UNREACHABLE
#undef SIR_UNREACHABLE
#endif

#if defined( _MSC_VER )
#include <sal.h>
#define SIR_ASSERT_PRINTF_STRING _Printf_format_string_
#else
#define SIR_ASSERT_PRINTF_STRING
#endif
#if defined( __GNUC__ ) || defined( __clang__ )
#define SIR_ASSERT_PRINTF_FORMAT __attribute__( ( format( printf, 2, 3 ) ) )
#else
#define SIR_ASSERT_PRINTF_FORMAT
#endif

class SIRAssert final
{
public:
	explicit SIRAssert( void ) = delete;
	explicit SIRAssert( const SIRAssert& ) = delete;
	explicit SIRAssert( SIRAssert&& ) = delete;

	SIRAssert& operator=( const SIRAssert& ) = delete;
	SIRAssert& operator=( SIRAssert&& ) = delete;

	[[noreturn]] static void fail( const sirAssertContext_t& context, const char *pszMessage ) noexcept;
	static bool reportEnsure( const sirAssertContext_t& context, const char *pszMessage ) noexcept;
	[[nodiscard]] static bool claimEnsure( std::atomic<uint32_t>& nClaim ) noexcept;

	// Macro implementation: beginFailure must run before message arguments.
	static void beginFailure( void ) noexcept;
	[[noreturn]] static void
	failFormatted( const sirAssertContext_t& context, SIR_ASSERT_PRINTF_STRING const char *pszFormat, ... ) noexcept SIR_ASSERT_PRINTF_FORMAT;
	static bool
	reportEnsureFormatted( const sirAssertContext_t& context, SIR_ASSERT_PRINTF_STRING const char *pszFormat, ... ) noexcept SIR_ASSERT_PRINTF_FORMAT;
};

#undef SIR_ASSERT_PRINTF_FORMAT
#undef SIR_ASSERT_PRINTF_STRING

// Hidden visibility keeps ELF sites local to the linked image, including
// libraries built with GNU unique symbols. Template storage coalesces across TUs.
template <uint64_t nFileId, uint32_t nLine>
class SIR_HIDDEN SIRAssertSite final
{
	static_assert( nFileId != 0, "Map __FILE__ to a canonical repository-relative path before using SIR_ENSURE" );

public:
	explicit SIRAssertSite( void ) = delete;
	explicit SIRAssertSite( const SIRAssertSite& ) = delete;
	explicit SIRAssertSite( SIRAssertSite&& ) = delete;

	SIRAssertSite& operator=( const SIRAssertSite& ) = delete;
	SIRAssertSite& operator=( SIRAssertSite&& ) = delete;

	[[nodiscard]] static bool claim( void ) noexcept;

private:
	static std::atomic<uint32_t> s_nClaim;
};

template <uint64_t nFileId, uint32_t nLine>
std::atomic<uint32_t> SIRAssertSite<nFileId, nLine>::s_nClaim{ 0 };

template <uint64_t nFileId, uint32_t nLine>
bool SIRAssertSite<nFileId, nLine>::claim( void ) noexcept
{
	return SIRAssert::claimEnsure( s_nClaim );
}

#define SIR_ASSERT_DETAIL_CONTEXT( expressionText ) \
	sirAssertContext_t \
	{ \
		expressionText, SIR_DIAGNOSTIC_SOURCE_LOCATION \
	}
#define SIR_ASSERT_DETAIL_CLAIM() SIRAssertSite<SIRDiagnosticFileIdentity::hash( __FILE__ ), static_cast<uint32_t>( __LINE__ )>::claim()
#define SIR_ASSERT_DETAIL_FATAL_MESSAGE( expressionText, ... ) \
	( SIRAssert::beginFailure(), SIRAssert::failFormatted( SIR_ASSERT_DETAIL_CONTEXT( expressionText ), __VA_ARGS__ ) )

#define SIR_CHECK( expression ) \
	( static_cast<bool>( expression ) ? static_cast<void>( 0 ) : SIRAssert::fail( SIR_ASSERT_DETAIL_CONTEXT( #expression ), nullptr ) )
#define SIR_CHECK_MSG( expression, ... ) \
	( static_cast<bool>( expression ) ? static_cast<void>( 0 ) : SIR_ASSERT_DETAIL_FATAL_MESSAGE( #expression, __VA_ARGS__ ) )

#if SIR_DIAGNOSTIC_ASSERT_ENABLED
#define SIR_ASSERT( expression ) SIR_CHECK( expression )
#define SIR_ASSERT_MSG( expression, ... ) SIR_CHECK_MSG( expression, __VA_ARGS__ )
#define SIR_VERIFY( expression ) SIR_CHECK( expression )
#define SIR_VERIFY_MSG( expression, ... ) SIR_CHECK_MSG( expression, __VA_ARGS__ )
#else
#define SIR_ASSERT( expression ) static_cast<void>( 0 )
#define SIR_ASSERT_MSG( expression, ... ) static_cast<void>( 0 )
#define SIR_VERIFY( expression ) static_cast<void>( static_cast<bool>( expression ) )
#define SIR_VERIFY_MSG( expression, ... ) static_cast<void>( static_cast<bool>( expression ) )
#endif

// Each ensure-once expansion requires its own physical source line.
#define SIR_ENSURE( expression ) \
	( static_cast<bool>( expression ) || ( SIR_ASSERT_DETAIL_CLAIM() && SIRAssert::reportEnsure( SIR_ASSERT_DETAIL_CONTEXT( #expression ), nullptr ) ) )
#define SIR_ENSURE_MSG( expression, ... ) \
	( static_cast<bool>( expression ) || \
	  ( SIR_ASSERT_DETAIL_CLAIM() && SIRAssert::reportEnsureFormatted( SIR_ASSERT_DETAIL_CONTEXT( #expression ), __VA_ARGS__ ) ) )
#define SIR_ENSURE_ALWAYS( expression ) ( static_cast<bool>( expression ) || SIRAssert::reportEnsure( SIR_ASSERT_DETAIL_CONTEXT( #expression ), nullptr ) )
#define SIR_ENSURE_ALWAYS_MSG( expression, ... ) \
	( static_cast<bool>( expression ) || SIRAssert::reportEnsureFormatted( SIR_ASSERT_DETAIL_CONTEXT( #expression ), __VA_ARGS__ ) )

#define SIR_UNREACHABLE() SIRAssert::fail( SIR_ASSERT_DETAIL_CONTEXT( "unreachable" ), nullptr )
#define SIR_UNIMPLEMENTED() SIRAssert::fail( SIR_ASSERT_DETAIL_CONTEXT( "unimplemented" ), nullptr )
#define SIR_UNREACHABLE_MSG( ... ) SIR_ASSERT_DETAIL_FATAL_MESSAGE( "unreachable", __VA_ARGS__ )
#define SIR_UNIMPLEMENTED_MSG( ... ) SIR_ASSERT_DETAIL_FATAL_MESSAGE( "unimplemented", __VA_ARGS__ )

#endif // __SIR_ASSERT_H__
