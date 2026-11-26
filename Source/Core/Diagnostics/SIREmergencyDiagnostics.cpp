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

#include <Core/Diagnostics/SIRDiagnosticBuffer.h>
#include <Core/Diagnostics/SIREmergencyDiagnostics.h>
#include <Core/Platform/Shared/Diagnostics/SIRPlatformDebug.h>

std::atomic<uint32_t> SIREmergencyDiagnostics::s_nTerminating{ 0 };

void sirDiagnosticBuffer_s::appendNumber( const uint64_t nValue, const uint8_t nBase ) noexcept
{
	char szNumber[32]{};
	const std::to_chars_result result = std::to_chars( szNumber, szNumber + sizeof( szNumber ), nValue, nBase );
	if ( result.ec == std::errc{} ) {
		append( szNumber, static_cast<uint32_t>( result.ptr - szNumber ) );
	}
}

bool SIREmergencyDiagnostics::writeRecord( const sirEmergencyRecord_t& data ) noexcept
{
	char szOutput[1024]{};
	sirDiagnosticBuffer_t output{ szOutput, sizeof( szOutput ), 0 };
	output.append( data.eSeverity == sirEmergencySeverity::SIR_EMERGENCY_FATAL ? "SIR fatal: " : "SIR ensure: " );
	output.append( data.szFile, sizeof( data.szFile ) );
	output.put( ':' );
	output.appendNumber( data.nLine );
	output.append( " in " );
	output.append( data.szFunction, sizeof( data.szFunction ) );
	output.append( ": " );
	output.append( data.szExpression, sizeof( data.szExpression ) );
	if ( data.szMessage[0] != '\0' ) {
		output.append( ": " );
		output.append( data.szMessage, sizeof( data.szMessage ) );
	}
	if ( data.nProcessId != 0 ) {
		output.append( " [pid=" );
		output.appendNumber( data.nProcessId );
		output.append( " tid=" );
		output.appendNumber( data.nThreadId );
		output.put( ']' );
	}
	if ( data.nReturnAddress != 0 ) {
		output.append( " [return=0x" );
		output.appendNumber( data.nReturnAddress, 16 );
		output.put( ']' );
	}
	output.put( '\n' );

	const sirDiagnosticResult eResult = SIRPlatformDebug::writeEmergency( { output.pData, output.nSize } );
	return eResult == sirDiagnosticResult::SIR_DIAGNOSTIC_OK || eResult == sirDiagnosticResult::SIR_DIAGNOSTIC_PARTIAL;
}

bool SIREmergencyDiagnostics::record( const sirEmergencyRecord_t& data ) noexcept
{
	return writeRecord( data );
}

void SIREmergencyDiagnostics::beginTermination( void ) noexcept
{
	// No payload is published through this flag. A recursive or competing
	// failure exits immediately, even when the owner is blocked in the sink.
	if constexpr ( std::atomic<uint32_t>::is_always_lock_free ) {
		uint32_t nExpected = 0;
		if ( s_nTerminating.compare_exchange_strong( nExpected, 1, std::memory_order_relaxed ) ) {
			return;
		}
	}
	SIRPlatformDebug::terminateImmediately();
}

void SIREmergencyDiagnostics::finishTermination( const sirEmergencyRecord_t& data ) noexcept
{
	static_cast<void>( writeRecord( data ) );
	SIRPlatformDebug::terminateImmediately();
}

void SIREmergencyDiagnostics::terminate( const sirEmergencyRecord_t& data ) noexcept
{
	beginTermination();
	finishTermination( data );
}
