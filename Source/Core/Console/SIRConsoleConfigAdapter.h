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

#ifndef __SIRENGINE_SIRCONSOLECONFIGADAPTER_H__
#define __SIRENGINE_SIRCONSOLECONFIGADAPTER_H__

#include <SIRPCH.hpp>
#include <Core/Console/SIRConsoleService.h>
#include <Core/CVars/SIRCVarConfigAdapter.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

typedef struct sirConsoleConfigReadRequest_s
{
	const char *pszName;
	uint32_t nNameBytes;
	uint8_t *pBytes;
	uint32_t nCapacity;
	uint32_t nRead;
} sirConsoleConfigReadRequest_t;

typedef sirConfigResult_t ( *sirConsoleConfigRead_t )( void *pContext, sirConsoleConfigReadRequest_t *pRequest ) noexcept;

typedef struct sirConsoleConfigStorage_s
{
	uint8_t *pFileBytes;
	uint32_t nFileCapacity;
	sirConsoleConfigRead_t pfnRead;
	void *pContext;
} sirConsoleConfigStorage_t;

// The reader callback owns path resolution/authorization and may use a VFS or native file.
// No second CFG tokenizer is introduced: exec passes INI bytes to the Core/Config codec.
class SIRConsoleConfigAdapter
{
public:
	SIRConsoleConfigAdapter( void ) noexcept;
	SIRConsoleConfigAdapter( const SIRConsoleConfigAdapter& ) = delete;
	~SIRConsoleConfigAdapter( void ) noexcept;

	SIRConsoleConfigAdapter& operator=( const SIRConsoleConfigAdapter& ) = delete;

	const sirConfigDiagnostic_t& getLastDiagnostic( void ) const noexcept;

	sirConfigResult_t initialize( SIRCVarConfigAdapter& adapter, const sirConsoleConfigStorage_t& storage ) noexcept;
	void shutdown( void ) noexcept;

	static sirRegistrationResult_t execute( void *pContext, const char *pszFileName, uint32_t nBytes,
	                                        const sirConsoleSubmission_t& submission ) noexcept;

protected:
private:
	SIRCVarConfigAdapter *m_pAdapter;
	sirConsoleConfigStorage_t m_Storage;
	sirConfigDiagnostic_t m_LastDiagnostic;
	bool m_bExecuting;
};

#endif // __SIRENGINE_SIRCONSOLECONFIGADAPTER_H__
