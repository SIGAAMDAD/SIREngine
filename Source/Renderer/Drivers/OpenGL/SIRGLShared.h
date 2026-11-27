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

#ifndef __SIR_GL_SHARED_H__
#define __SIR_GL_SHARED_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
    #pragma once
#endif
// clang-format on

#include <GL/gl.h>
#include <GL/glext.h>
#include "SIRGLMissingTypes.h"

// The calling thread must have the context current during load and use.
// One table belongs to one GL context (or a verified sharing-compatible group).
// The resolver must remain valid until loading completes.
struct SIRGLProcResolver
{
	void *user = nullptr;
	void *( *resolve )( void *user, const char *name ) noexcept = nullptr;
};

enum class SIRGLLoadResult
{
	SIR_GL_SUCCESS,
	SIR_GL_INVALID_RESOLVER,
	SIR_GL_INVALID_NATIVE_CONTEXT,
	SIR_GL_WRONG_THREAD,
	SIR_GL_NO_CURRENT_CONEXT,
	SIR_GL_UNSUPPORTED_API,
	SIR_GL_MISSING_REQUIRED_PROCEDURE
};

struct SIRGLLoadError
{
	SIRGLLoadResult result = SIRGLLoadResult::SIR_GL_SUCCESS;
	const char *procedure = nullptr;
	int nMajor = 0;
	int nMinor = 0;
};

struct SIRGLProcs
{
	int nMajor = 0;
	int nMinor = 0;
	bool bCompatibility = false;

	// The inventory contains extension strings needed to choose entrypoints.
	// A bitset is cheaper than storing an allocation-backed extension catalog.
	enum : size_t
	{
		KnownExtensionCount = 0
#define SIR_GL_KNOWN_EXTENSION( Name ) +1
#include "SIRGLKnownExtensions.inl"
#undef SIR_GL_KNOWN_EXTENSION
	};
	uint64_t m_szKnownExtensionBits[( KnownExtensionCount + 63 ) / 64] = {};
	bool m_bLoaded = false;

#define SIR_GL_VERSION( Major, Minor )
#define SIR_GL_EXTENSION( Name )
#define SIR_GL_PROC( Method, Proc, PfnType, ReturnType, Parameters, Arguments ) PfnType Proc = nullptr;
#include "SIRGLCoreProcs.inl"
#include "SIRGLCompatibilityProcs.inl"
#include "SIRGLExtensionProcs.inl"
#undef SIR_GL_PROC
#undef SIR_GL_EXTENSION
#undef SIR_GL_VERSION

	inline void reset( void ) noexcept
	{
		*this = {};
	}

	inline bool atLeast( int nRequiredMajor, int nRequiredMinor ) const noexcept
	{
		return nMajor > nRequiredMajor || ( nMajor == nRequiredMajor && nMinor >= nRequiredMinor );
	}

	// Exact extension-string lookup. Known names are served from the cached
	// registry. Unknown extension names are queried directly on the current
	// context so capabilities without entrypoints are also supported.
	inline bool hasExtension( const char *pszExtensionName ) const noexcept
	{
		if ( !m_bLoaded || !validExtensionName( pszExtensionName ) ) {
			return false;
		}

		const char *const *pszKnownNames = knownExtensionNames();
		for ( size_t iName = 0; iName < KnownExtensionCount; ++iName ) {
			if ( strcmp( pszKnownNames[iName], pszExtensionName ) == 0 ) {
				return ( m_szKnownExtensionBits[iName / 64] & ( UINT64_C( 1 ) << ( iName % 64 ) ) ) != 0;
			}
		}

		if ( atLeast( 3, 0 ) && glGetStringi != nullptr && glGetIntegerv != nullptr ) {
			GLint nCount = 0;
			glGetIntegerv( GL_NUM_EXTENSIONS, &nCount );

			for ( GLint iExtension = 0; iExtension < nCount; ++iExtension ) {
				const GLubyte *pFound = glGetStringi( GL_EXTENSIONS, ( GLuint )iExtension );
				if ( pFound != nullptr && strcmp( ( const char * )pFound, pszExtensionName ) == 0 ) {
					return true;
				}
			}

			return false;
		}

		if ( glGetString == nullptr ) {
			return false;
		}

		const char *pszAll = ( const char * )glGetString( GL_EXTENSIONS );
		return extensionInList( pszAll, pszExtensionName );
	}

	inline bool load( const SIRGLProcResolver& resolver, SIRGLLoadError *error = nullptr ) noexcept
	{
		if ( error != nullptr ) {
			*error = {};
		}
		if ( resolver.resolve == nullptr ) {
			return fail( error, SIRGLLoadResult::SIR_GL_INVALID_RESOLVER, nullptr, nullptr );
		}
		// Resolve only bootstrap functions before querying the actual context.
		const PFNGLGETSTRINGPROC bootstrapGetString = reinterpret_cast<PFNGLGETSTRINGPROC>( resolveSafe( resolver, "glGetString" ) );
		const PFNGLGETINTEGERVPROC bootstrapGetIntegerv =
		    reinterpret_cast<PFNGLGETINTEGERVPROC>( resolveSafe( resolver, "glGetIntegerv" ) );
		if ( bootstrapGetString == nullptr || bootstrapGetIntegerv == nullptr ) {
			return fail( error, SIRGLLoadResult::SIR_GL_NO_CURRENT_CONEXT, nullptr, nullptr );
		}
		const char *version = reinterpret_cast<const char *>( bootstrapGetString( GL_VERSION ) );
		if ( version == nullptr ) {
			return fail( error, SIRGLLoadResult::SIR_GL_NO_CURRENT_CONEXT, nullptr, nullptr );
		}
		// Desktop OpenGL only; do not treat an OpenGL ES version as desktop GL.
		if ( strncmp( version, "OpenGL ES", 9 ) == 0 ) {
			return fail( error, SIRGLLoadResult::SIR_GL_UNSUPPORTED_API, nullptr, nullptr );
		}
		int detectedMajor = 0;
		int detectedMinor = 0;
		const char *cursor = version;
		while ( *cursor >= '0' && *cursor <= '9' ) {
			detectedMajor = detectedMajor * 10 + ( *cursor - '0' );
			++cursor;
		}
		if ( *cursor != '.' || detectedMajor < 1 ) {
			return fail( error, SIRGLLoadResult::SIR_GL_UNSUPPORTED_API, nullptr, nullptr );
		}
		++cursor;
		if ( *cursor < '0' || *cursor > '9' ) {
			return fail( error, SIRGLLoadResult::SIR_GL_UNSUPPORTED_API, nullptr, nullptr );
		}
		while ( *cursor >= '0' && *cursor <= '9' ) {
			detectedMinor = detectedMinor * 10 + ( *cursor - '0' );
			++cursor;
		}
		SIRGLProcs pending{};
		pending.nMajor = detectedMajor;
		pending.nMinor = detectedMinor;
		// 3.2+ advertises explicit core/compatibility profiles. OpenGL 3.1
		// requires ARB_compatibility to restore the removed fixed pipeline;
		// 3.0 forward-compatible contexts exclude deprecated entrypoints.
		if ( pending.atLeast( 3, 2 ) ) {
			GLint nProfile = 0;
			bootstrapGetIntegerv( GL_CONTEXT_PROFILE_MASK, &nProfile );
			pending.bCompatibility = ( nProfile & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT ) != 0;
		} else if ( !pending.atLeast( 3, 0 ) ) {
			pending.bCompatibility = true;
		} else {
			GLint nFlags = 0;
			bootstrapGetIntegerv( GL_CONTEXT_FLAGS, &nFlags );
			pending.bCompatibility = ( nFlags & GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT ) == 0;
		}
		bool required = false;
		const char *missing = nullptr;
		bool ok = true;
#define SIR_GL_VERSION( Major, Minor ) required = pending.atLeast( ( Major ), ( Minor ) );
#define SIR_GL_EXTENSION( Name )
#define SIR_GL_PROC( Method, Proc, PfnType, ReturnType, Parameters, Arguments )                                                            \
	if ( required ) {                                                                                                                      \
		pending.Proc = reinterpret_cast<PfnType>( resolveSafe( resolver, #Proc ) );                                                        \
		if ( pending.Proc == nullptr && ok ) {                                                                                             \
			ok = false;                                                                                                                    \
			missing = #Proc;                                                                                                               \
		}                                                                                                                                  \
	}
#include "SIRGLCoreProcs.inl"
#undef SIR_GL_PROC
#undef SIR_GL_EXTENSION
#undef SIR_GL_VERSION
		if ( !ok ) {
			return fail( error, SIRGLLoadResult::SIR_GL_MISSING_REQUIRED_PROCEDURE, missing, &pending );
		}
		// Enumerate the actual driver extension inventory once, with no heap
		// allocations and no repeated glGetStringi calls per proc group.
		pending.cacheKnownExtensions();
		pending.m_bLoaded = true;

		if ( pending.nMajor == 3 && pending.nMinor == 1 ) {
			pending.bCompatibility = pending.bCompatibility && pending.hasExtension( "GL_ARB_compatibility" );
		}

		if ( pending.bCompatibility ) {
#define SIR_GL_VERSION( Major, Minor ) required = pending.atLeast( ( Major ), ( Minor ) );
#define SIR_GL_EXTENSION( Name )
#define SIR_GL_PROC( Method, Proc, PfnType, ReturnType, Parameters, Arguments )                                                            \
	if ( required ) {                                                                                                                      \
		pending.Proc = reinterpret_cast<PfnType>( resolveSafe( resolver, #Proc ) );                                                        \
	}
#include "SIRGLCompatibilityProcs.inl"
#undef SIR_GL_PROC
#undef SIR_GL_EXTENSION
#undef SIR_GL_VERSION
		}
		// An advertised extension may lack optional driver entrypoints.
		// Such function pointers remain null; callers must check before use.
		bool enabled = false;
#define SIR_GL_VERSION( Major, Minor )
#define SIR_GL_EXTENSION( Name ) enabled = pending.hasExtension( Name );
#define SIR_GL_PROC( Method, Proc, PfnType, ReturnType, Parameters, Arguments )                                                            \
	if ( enabled ) {                                                                                                                       \
		pending.Proc = reinterpret_cast<PfnType>( resolveSafe( resolver, #Proc ) );                                                        \
	}
#include "SIRGLExtensionProcs.inl"
#undef SIR_GL_PROC
#undef SIR_GL_EXTENSION
#undef SIR_GL_VERSION
		// Core procedure names can also be exported by extensions on older GL.
		// For example GL_ARB_direct_state_access before GL 4.5 and KHR_debug
		// before GL 4.3. A single pointer slot serves both entry paths.
#define SIR_GL_VERSION( Major, Minor )
#define SIR_GL_EXTENSION( Name ) enabled = pending.hasExtension( Name );
#define SIR_GL_PROC( Method, Proc, PfnType, ReturnType, Parameters, Arguments )                                                            \
	if ( enabled && pending.Proc == nullptr ) {                                                                                            \
		pending.Proc = reinterpret_cast<PfnType>( resolveSafe( resolver, #Proc ) );                                                        \
	}
#include "SIRGLPromotedCoreProcs.inl"
#undef SIR_GL_PROC
#undef SIR_GL_EXTENSION
#undef SIR_GL_VERSION
		*this = pending;
		return true;
	}

private:
	static inline bool validExtensionName( const char *pszName ) noexcept
	{
		if ( pszName == nullptr || pszName[0] == '\0' ) {
			return false;
		}

		return strpbrk( pszName, " \t\r\n\f\v" ) == nullptr;
	}

	static inline bool extensionInList( const char *pszAll, const char *pszName ) noexcept
	{
		if ( pszAll == nullptr || pszName == nullptr ) {
			return false;
		}

		const size_t nNameLength = strlen( pszName );
		while ( *pszAll != '\0' ) {
			while ( *pszAll == ' ' ) {
				++pszAll;
			}

			const char *pszStart = pszAll;
			while ( *pszAll != '\0' && *pszAll != ' ' ) {
				++pszAll;
			}

			const size_t nLength = ( size_t )( pszAll - pszStart );
			if ( nLength == nNameLength && memcmp( pszStart, pszName, nLength ) == 0 ) {
				return true;
			}
		}

		return false;
	}

	static inline const char *const *knownExtensionNames( void ) noexcept
	{
		static const char *const s_pszNames[] = {
#define SIR_GL_KNOWN_EXTENSION( Name ) Name,
#include "SIRGLKnownExtensions.inl"
#undef SIR_GL_KNOWN_EXTENSION
		};
		return s_pszNames;
	}

	inline void markKnownExtension( const char *pszName, size_t nLength ) noexcept
	{
		if ( pszName == nullptr || nLength == 0 ) {
			return;
		}

		const char *const *pszNames = knownExtensionNames();
		for ( size_t iName = 0; iName < KnownExtensionCount; ++iName ) {
			if ( strlen( pszNames[iName] ) == nLength && memcmp( pszNames[iName], pszName, nLength ) == 0 ) {
				m_szKnownExtensionBits[iName / 64] |= UINT64_C( 1 ) << ( iName % 64 );
				return;
			}
		}
	}

	inline void cacheKnownExtensions( void ) noexcept
	{
		if ( atLeast( 3, 0 ) && glGetStringi != nullptr && glGetIntegerv != nullptr ) {
			GLint nCount = 0;
			glGetIntegerv( GL_NUM_EXTENSIONS, &nCount );
			for ( GLint iExtension = 0; iExtension < nCount; ++iExtension ) {
				const char *pszName = ( const char * )glGetStringi( GL_EXTENSIONS, ( GLuint )iExtension );
				if ( pszName != nullptr ) {
					markKnownExtension( pszName, strlen( pszName ) );
				}
			}
			return;
		}

		const char *pszAll = glGetString != nullptr ? ( const char * )glGetString( GL_EXTENSIONS ) : nullptr;
		while ( pszAll != nullptr && *pszAll != '\0' ) {
			while ( *pszAll == ' ' ) {
				++pszAll;
			}
			const char *pszStart = pszAll;
			while ( *pszAll != '\0' && *pszAll != ' ' ) {
				++pszAll;
			}
			markKnownExtension( pszStart, ( size_t )( pszAll - pszStart ) );
		}
	}

	static inline void *resolveSafe( const SIRGLProcResolver& resolver, const char *pszName ) noexcept
	{
		void *pProc = resolver.resolve( resolver.user, pszName );
		const uintptr_t nAddress = ( uintptr_t )pProc;
		if ( nAddress <= 3 || nAddress == UINTPTR_MAX ) {
			return nullptr;
		}
		return pProc;
	}

	static inline bool fail( SIRGLLoadError *pError, SIRGLLoadResult eResult, const char *pszProcedure,
	                         const SIRGLProcs *pTable ) noexcept
	{
		if ( pError != nullptr ) {
			pError->result = eResult;
			pError->procedure = pszProcedure;
			pError->nMajor = pTable != nullptr ? pTable->nMajor : 0;
			pError->nMinor = pTable != nullptr ? pTable->nMinor : 0;
		}
		return false;
	}
};

#endif // __SIR_GL_SHARED_H__
