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

#ifndef __SIR_PLATFORM_LOG_CONSOLE_SINK_H__
#define __SIR_PLATFORM_LOG_CONSOLE_SINK_H__
#include <Core/Diagnostics/Logging/SIRLoggerSink.h>
class SIRPlatformLogConsoleSink final : public SIRLoggerSink
{
public:
	SIRPlatformLogConsoleSink( void ) noexcept = default;
	~SIRPlatformLogConsoleSink( void ) noexcept override = default;
	sirLogSinkResult write( const sirLogRecord_t& record ) noexcept override;
	sirLogSinkResult flush( void ) noexcept override;
};
#endif
