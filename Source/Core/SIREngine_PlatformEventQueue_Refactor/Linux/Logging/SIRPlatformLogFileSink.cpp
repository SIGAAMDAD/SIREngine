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

#include <Core/Platform/Shared/Logging/SIRPlatformLogFileSink.h>
#include <cstdio>

SIRPlatformLogFileSink::SIRPlatformLogFileSink( void ) noexcept = default;

SIRPlatformLogFileSink::~SIRPlatformLogFileSink( void ) noexcept
{
	static_cast<void>( flush() );
	if ( m_pFile != nullptr )
		std::fclose( static_cast<FILE *>( m_pFile ) );
}

bool SIRPlatformLogFileSink::initialize( const char *pszPath ) noexcept
{
	m_pFile = pszPath == nullptr ? nullptr : std::fopen( pszPath, "ab" );
	return m_pFile != nullptr;
}

sirLogSinkResult SIRPlatformLogFileSink::write( const sirLogRecord_t& record ) noexcept
{
	if ( m_pFile == nullptr )
		return sirLogSinkResult::SIR_LOG_SINK_INVALID;
	const size_t n = std::fwrite( record.szMessage, 1, record.nMessageBytes, static_cast<FILE *>( m_pFile ) );
	if ( n != record.nMessageBytes || std::fputc( '\n', static_cast<FILE *>( m_pFile ) ) == EOF )
		return sirLogSinkResult::SIR_LOG_SINK_ERROR;
	return sirLogSinkResult::SIR_LOG_SINK_OK;
}

sirLogSinkResult SIRPlatformLogFileSink::flush( void ) noexcept
{
	return m_pFile == nullptr || std::fflush( static_cast<FILE *>( m_pFile ) ) == 0 ? sirLogSinkResult::SIR_LOG_SINK_OK : sirLogSinkResult::SIR_LOG_SINK_ERROR;
}
