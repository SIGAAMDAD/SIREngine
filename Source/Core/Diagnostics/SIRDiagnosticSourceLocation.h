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

#ifndef __SIR_DIAGNOSTICSOURCELOCATION_H__
#define __SIR_DIAGNOSTICSOURCELOCATION_H__

#if defined( SIR_PRAGMA_ONCE_SUPPORTED ) && SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Core/SIRPCH.h>

typedef struct sirDiagnosticSourceLocation_s
{
	const char *pszFile;
	const char *pszFunction;
	uint32_t nLine;
} sirDiagnosticSourceLocation_t;

#define SIR_DIAGNOSTIC_SOURCE_LOCATION \
	sirDiagnosticSourceLocation_t \
	{ \
		SIRDiagnosticFileIdentity::canonical( __FILE__ ), __func__, static_cast<uint32_t>( __LINE__ ) \
	}

class SIRDiagnosticFileIdentity final
{
public:
	explicit SIRDiagnosticFileIdentity( void ) = delete;
	explicit SIRDiagnosticFileIdentity( const SIRDiagnosticFileIdentity& ) = delete;
	explicit SIRDiagnosticFileIdentity( SIRDiagnosticFileIdentity&& ) = delete;

	SIRDiagnosticFileIdentity& operator=( const SIRDiagnosticFileIdentity& ) = delete;
	SIRDiagnosticFileIdentity& operator=( SIRDiagnosticFileIdentity&& ) = delete;

	// Targets map the repository prefix away before compiling. Native path
	// separators and an optional leading "./" have the same identity.
	[[nodiscard]] static constexpr uint64_t hash( const char *pszFile ) noexcept;
	[[nodiscard]] static constexpr const char *canonical( const char *pszFile ) noexcept;

private:
	[[nodiscard]] static constexpr char normalize( char nCharacter ) noexcept;
	[[nodiscard]] static constexpr bool startsWith( const char *pszFile, const char *pszPrefix ) noexcept;
};

constexpr char SIRDiagnosticFileIdentity::normalize( char nCharacter ) noexcept
{
	return nCharacter == '\\' ? '/' : nCharacter;
}

constexpr bool SIRDiagnosticFileIdentity::startsWith( const char *pszFile, const char *pszPrefix ) noexcept
{
	while ( *pszPrefix != '\0' ) {
		if ( normalize( *pszFile++ ) != *pszPrefix++ ) {
			return false;
		}
	}
	return true;
}

constexpr uint64_t SIRDiagnosticFileIdentity::hash( const char *pszFile ) noexcept
{
	pszFile = canonical( pszFile );
	if ( startsWith( pszFile, "./" ) ) {
		pszFile += 2;
	}
	if ( !startsWith( pszFile, "Source/" ) && !startsWith( pszFile, "Tests/" ) && !startsWith( pszFile, "Tools/" ) ) {
		return 0;
	}

	uint64_t nHash = UINT64_C( 14695981039346656037 );
	bool bSegmentStart = true;
	for ( const char *p = pszFile; *p != '\0'; ++p ) {
		const char nCharacter = normalize( *p );
		if ( nCharacter == ':' ) {
			return 0;
		}
		if ( bSegmentStart ) {
			if ( nCharacter == '/' || startsWith( p, "./" ) || startsWith( p, "../" ) ) {
				return 0;
			}
			if ( nCharacter == '.' && ( p[1] == '\0' || ( p[1] == '.' && p[2] == '\0' ) ) ) {
				return 0;
			}
		}
		bSegmentStart = nCharacter == '/';
		nHash = ( nHash ^ static_cast<uint8_t>( nCharacter ) ) * UINT64_C( 1099511628211 );
	}
	return bSegmentStart ? 0 : nHash;
}

constexpr const char *SIRDiagnosticFileIdentity::canonical( const char *pszFile ) noexcept
{
	const char *pCandidate = pszFile;
	for ( const char *pCursor = pszFile; *pCursor != '\0'; ++pCursor ) {
		if ( ( *pCursor == '/' || *pCursor == '\\' ) &&
		     ( startsWith( pCursor + 1, "Source/" ) || startsWith( pCursor + 1, "Tests/" ) || startsWith( pCursor + 1, "Tools/" ) ) ) {
			pCandidate = pCursor + 1;
		}
	}
	return pCandidate;
}

#endif // __SIR_DIAGNOSTICSOURCELOCATION_H__
