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

#include <Core/Platform/Shared/Logging/SIRPlatformLogConsoleSink.h>
#include <cstdio>

sirLogSinkResult SIRPlatformLogConsoleSink::write( const sirLogRecord_t& record ) noexcept
{
	const size_t n = std::fwrite( record.szMessage, 1, record.nMessageBytes, stderr );
	if ( n != record.nMessageBytes || std::fputc( '\n', stderr ) == EOF )
		return sirLogSinkResult::SIR_LOG_SINK_ERROR;
	return sirLogSinkResult::SIR_LOG_SINK_OK;
}

sirLogSinkResult SIRPlatformLogConsoleSink::flush( void ) noexcept
{
	return std::fflush( stderr ) == 0 ? sirLogSinkResult::SIR_LOG_SINK_OK : sirLogSinkResult::SIR_LOG_SINK_ERROR;
}
