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

#ifndef __SIR_LOGGERSYSTEM_H__
#define __SIR_LOGGERSYSTEM_H__

#include <Core/Diagnostics/Logging/SIRLoggerSink.h>
#include <Core/Platform/Shared/Threading/SIRThread.h>
#include <Core/Systems/SIREngineSystem.h>
#include <Core/Platform/Shared/Logging/SIRPlatformLogFileSink.h>
#include <Core/Platform/Shared/Logging/SIRPlatformLogConsoleSink.h>

class SIRLoggerSystem final : public SIREngineSystem, public SIRLoggerSink
{
public:
	explicit SIRLoggerSystem( const sirLoggerSystemConfig_t& config ) noexcept;
	~SIRLoggerSystem( void ) noexcept override;
	SIRLoggerSystem( const SIRLoggerSystem& ) = delete;
	SIRLoggerSystem( SIRLoggerSystem&& ) = delete;
	SIRLoggerSystem& operator=( const SIRLoggerSystem& ) = delete;
	SIRLoggerSystem& operator=( SIRLoggerSystem&& ) = delete;

	[[nodiscard]] sirLogSinkResult write( const sirLogRecord_t& record ) noexcept override;
	[[nodiscard]] sirLogSinkResult flush( void ) noexcept override;
	[[nodiscard]] sirLogSinkResult attachSink( SIRLoggerSink& sink ) noexcept;
	[[nodiscard]] sirLogSinkResult detachSink( SIRLoggerSink& sink ) noexcept;
	SIRSystemResult initialize( void ) noexcept override;
	void update( float fDeltaSeconds ) noexcept override;
	SIRSystemResult shutdown( void ) noexcept override;
	[[nodiscard]] bool isInitialized( void ) const noexcept;

private:
	static constexpr uint32_t s_nMaxQueue = 256;
	static constexpr uint32_t s_nMaxSinks = 4;
	struct Slot { sirLogRecord_t record{}; };
	static void worker( void *pContext ) noexcept;
	void runWorker( void ) noexcept;
	void deliver( const sirLogRecord_t& record ) noexcept;

	sirLoggerSystemConfig_t m_Config;
	SIRThread m_Thread;
	Slot m_Slots[s_nMaxQueue];
	std::atomic<uint32_t> m_nWrite{ 0 };
	std::atomic<uint32_t> m_nRead{ 0 };
	std::atomic_flag m_ProducerGate = ATOMIC_FLAG_INIT;
	std::atomic<bool> m_bRunning{ false };
	std::atomic<bool> m_bAccepting{ false };
	SIRLoggerSink *m_pSinks[s_nMaxSinks]{};
	std::atomic<uint32_t> m_nInFlight[s_nMaxSinks]{};
	SIRPlatformLogFileSink m_File;
	SIRPlatformLogConsoleSink m_Console;
};

#endif
