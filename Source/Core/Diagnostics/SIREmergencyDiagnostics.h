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

#ifndef __SIR_EMERGENCYDIAGNOSTICS_H__
#define __SIR_EMERGENCYDIAGNOSTICS_H__

#if defined( SIR_PRAGMA_ONCE_SUPPORTED ) && SIR_PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#include <Core/SIRPCH.h>

enum class sirEmergencySeverity : uint8_t
{
	SIR_EMERGENCY_ENSURE,
	SIR_EMERGENCY_FATAL
};

// Owned, bounded data. Full text arrays need not be NUL-terminated.
typedef struct sirEmergencyRecord_s
{
	sirEmergencySeverity eSeverity;
	uint32_t nLine;
	uint32_t nProcessId;
	uint64_t nThreadId;
	uintptr_t nReturnAddress;
	char szFile[192];
	char szFunction[96];
	char szExpression[160];
	char szMessage[384];
} sirEmergencyRecord_t;

class SIREmergencyDiagnostics final
{
public:
	explicit SIREmergencyDiagnostics( void ) = delete;
	explicit SIREmergencyDiagnostics( const SIREmergencyDiagnostics& ) = delete;
	explicit SIREmergencyDiagnostics( SIREmergencyDiagnostics&& ) = delete;

	SIREmergencyDiagnostics& operator=( const SIREmergencyDiagnostics& ) = delete;
	SIREmergencyDiagnostics& operator=( SIREmergencyDiagnostics&& ) = delete;

	// Synchronous, best-effort submission on a normal thread. No setup needed.
	[[nodiscard]] static bool record( const sirEmergencyRecord_t& data ) noexcept;
	[[noreturn]] static void terminate( const sirEmergencyRecord_t& data ) noexcept;

private:
	friend class SIRAssert;

	static void beginTermination( void ) noexcept;
	[[noreturn]] static void finishTermination( const sirEmergencyRecord_t& data ) noexcept;
	[[nodiscard]] static bool writeRecord( const sirEmergencyRecord_t& data ) noexcept;

	static std::atomic<uint32_t> s_nTerminating;
};

#endif // __SIR_EMERGENCYDIAGNOSTICS_H__
