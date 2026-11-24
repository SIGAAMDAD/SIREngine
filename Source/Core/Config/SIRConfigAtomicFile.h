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

#ifndef __SIR_CONFIGATOMICFILE_H__
#define __SIR_CONFIGATOMICFILE_H__

#include <Core/SIRPCH.h>
#include <Core/Config/SIRConfigTypes.h>
#include <Core/Platform/Shared/File/SIRPlatformFile.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

// Native path backend for an eventual VFS resolver. Paths are supplied/validated by caller.
// Exclusive same-directory temp file is flushed, closed, then atomically replaced.
class SIRConfigAtomicFile
{
public:
	SIRConfigAtomicFile( void ) noexcept;
	SIRConfigAtomicFile( const SIRConfigAtomicFile& ) = delete;
	~SIRConfigAtomicFile( void ) noexcept;

	SIRConfigAtomicFile& operator=( const SIRConfigAtomicFile& ) = delete;

	sirConfigSink_t getSink( void ) noexcept;

	SIRConfigResult open( const char *pszTargetPath, const char *pszTempPath, uint64_t nMaxBytes ) noexcept;
	SIRConfigResult commit( void ) noexcept;
	void cancel( void ) noexcept;

	static SIRConfigResult read( const char *pszPath, uint8_t *pBytes, uint32_t nCapacity, uint32_t *pRead ) noexcept;

protected:
private:
	static SIRConfigResult sinkWrite( void *pContext, const uint8_t *pBytes, uint64_t nBytes ) noexcept;

	SIRPlatformFile m_File;
	const char *m_pszTargetPath;
	const char *m_pszTempPath;
	uint64_t m_nMaxBytes;
	uint64_t m_nWrittenBytes;
	bool m_bOpened;
	bool m_bFailed;
};

#endif // __SIR_CONFIGATOMICFILE_H__
