// SPDX-License-Identifier: MPL-2.0
// Include this file inside the public section of a device/driver type with
// `sirGLProcs_t m_Procs` accessible. Each wrapper has an explicit signature.
// The caller must ensure the procedure is supported and the context is current.
// All methods below are inline; no variadic forwarding or dynamic allocations.
// Extension queries are cached for recognized proc-gating extensions.
inline bool hasExtension( const char *pszExtensionName ) const noexcept
{
	return m_Procs.hasExtension( pszExtensionName );
}

inline bool atLeast( int nMajor, int nMinor ) const noexcept
{
	return m_Procs.atLeast( nMajor, nMinor );
}
#define SIR_GL_VERSION( Major, Minor )
#define SIR_GL_EXTENSION( Name )
#define SIR_GL_PROC( Method, Proc, PfnType, ReturnType, Parameters, Arguments )                                                            \
	inline ReturnType Method Parameters const noexcept                                                                                     \
	{                                                                                                                                      \
		return m_Procs.Proc Arguments;                                                                                                     \
	}
#include "SIRGLCoreProcs.inl"
#include "SIRGLCompatibilityProcs.inl"
#include "SIRGLExtensionProcs.inl"
#undef SIR_GL_PROC
#undef SIR_GL_EXTENSION
#undef SIR_GL_VERSION
