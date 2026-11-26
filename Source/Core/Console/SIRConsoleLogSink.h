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

#ifndef __SIR_CONSOLE_LOG_SINK_H__
#define __SIR_CONSOLE_LOG_SINK_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/Diagnostics/Logging/SIRLoggerSink.h>

class SIRConsoleLogSink final : public SIRLoggerSink
{
public:
	SIRConsoleLogSink( void ) noexcept = default;
	~SIRConsoleLogSink() noexcept override = default;
	SIRConsoleLogSink( const SIRConsoleLogSink& ) = delete;
	SIRConsoleLogSink( SIRConsoleLogSink&& ) = delete;
	SIRConsoleLogSink& operator=( const SIRConsoleLogSink& ) = delete;
	SIRConsoleLogSink& operator=( SIRConsoleLogSink&& ) = delete;
	bool initialize( uint32_t nCapacity ) noexcept;
	sirLogSinkResult write( const sirLogRecord_t& record ) noexcept override;
	sirLogSinkResult flush( void ) noexcept override;
	uint32_t drain( sirLogRecord_t *pRecords, uint32_t nCapacity ) noexcept;
	void reset( void ) noexcept;

private:
	static constexpr uint32_t s_nMaxRecords = 256;
	sirLogRecord_t m_Records[s_nMaxRecords]{};
	std::atomic<uint32_t> m_nWrite{ 0 };
	std::atomic<uint32_t> m_nRead{ 0 };
	uint32_t m_nCapacity{};
};
#endif // __SIR_CONSOLE_LOG_SINK_H__
