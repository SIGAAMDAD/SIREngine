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

#ifndef __SIRENGINE_SIRCVARCONFIGADAPTER_H__
#define __SIRENGINE_SIRCVARCONFIGADAPTER_H__

#include <SIRPCH.hpp>
#include <Core/Config/Formats/INI/SIRIni.h>
#include <Core/CVars/SIRCVarRegistry.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

typedef struct sirCVarConfigBinding_s
{
	sirRegistrationHandle_t registration;
	sirCVarHandle_t cvar;
} sirCVarConfigBinding_t;

typedef struct sirCVarConfigStorage_s
{
	sirIniStorage_t ini;
	sirCVarWrite_t *pWrites;
	uint32_t nWriteCapacity;
	const sirCVarConfigBinding_t *pBindings;
	uint32_t nBindingCount;
	sirConfigLimits_t limits;
} sirCVarConfigStorage_t;

// Single owner-thread Config/registration/CVar integration point.
// Caller owns INI scratch, write scratch, mapping and immutable Config input until return.
class SIRCVarConfigAdapter
{
public:
	SIRCVarConfigAdapter( void ) noexcept;
	SIRCVarConfigAdapter( const SIRCVarConfigAdapter& ) = delete;
	~SIRCVarConfigAdapter( void ) noexcept;

	SIRCVarConfigAdapter& operator=( const SIRCVarConfigAdapter& ) = delete;

	bool isInitialized( void ) const noexcept;

	sirConfigResult_t initialize( SIRRegistrationCatalog& registration, SIRCVarRegistry& cvars, SIRStringPool& pool,
	                              const sirCVarConfigStorage_t& storage ) noexcept;
	sirConfigResult_t applyIni( const sirConfigInput_t& input, const sirCVarOrigin_t& origin, sirConfigDiagnostic_t *pDiagnostic ) noexcept;
	sirConfigResult_t writeArchive( const sirConfigSink_t& sink, uint32_t nCapabilities, uint64_t nMaxBytes,
	                                uint32_t *pWrittenEntries ) noexcept;
	void shutdown( void ) noexcept;

protected:
private:
	const sirCVarConfigBinding_t *findBinding( sirRegistrationHandle_t registration ) const noexcept;

	SIRRegistrationCatalog *m_pRegistration;
	SIRCVarRegistry *m_pCVars;
	SIRStringPool *m_pPool;
	sirCVarConfigStorage_t m_Storage;
};

#endif // __SIRENGINE_SIRCVARCONFIGADAPTER_H__
