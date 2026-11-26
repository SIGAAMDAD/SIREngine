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

#ifndef __SIR_LOGGERSINK_H__
#define __SIR_LOGGERSINK_H__
#include <Core/Diagnostics/Logging/SIRLogTypes.h>
enum class sirLogSinkResult : uint8_t { SIR_LOG_SINK_OK, SIR_LOG_SINK_FULL, SIR_LOG_SINK_BUSY, SIR_LOG_SINK_ERROR, SIR_LOG_SINK_INVALID, SIR_LOG_SINK_UNSUPPORTED };
class SIRLoggerSink
{
public:
	SIRLoggerSink( void ) noexcept = default;
	SIRLoggerSink( const SIRLoggerSink& ) = delete;
	SIRLoggerSink( SIRLoggerSink&& ) = delete;
	virtual ~SIRLoggerSink( void ) noexcept = default;
	SIRLoggerSink& operator=( const SIRLoggerSink& ) = delete;
	SIRLoggerSink& operator=( SIRLoggerSink&& ) = delete;
	[[nodiscard]] virtual sirLogSinkResult write( const sirLogRecord_t& record ) noexcept = 0;
	[[nodiscard]] virtual sirLogSinkResult flush( void ) noexcept = 0;
};
#endif
