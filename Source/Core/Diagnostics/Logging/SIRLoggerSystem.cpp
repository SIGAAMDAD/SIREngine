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

#include <Core/Diagnostics/Logging/SIRLoggerSystem.h>
#include <Core/Diagnostics/Logging/SIRLogger.h>

static std::atomic<SIRLoggerSystem *> g_pLogger{ nullptr };
static thread_local bool g_bLoggerSinkCallback = false;

SIRLoggerSystem::SIRLoggerSystem( const sirLoggerSystemConfig_t& config ) noexcept : m_Config( config ) {}
SIRLoggerSystem::~SIRLoggerSystem( void ) noexcept { static_cast<void>( shutdown() ); }
bool SIRLoggerSystem::isInitialized( void ) const noexcept { return m_bAccepting.load( std::memory_order_acquire ); }

SIRSystemResult SIRLoggerSystem::initialize( void ) noexcept
{
	if ( isInitialized() ) return SIRSystemResult::SIR_SYSTEM_BUSY;
	if ( m_Config.nQueueCapacity == 0 || m_Config.nQueueCapacity > s_nMaxQueue ) return SIRSystemResult::SIR_SYSTEM_ERROR;
	if ( m_Config.bEnableFile && !m_File.initialize( m_Config.pszFilePath ) ) return SIRSystemResult::SIR_SYSTEM_ERROR;
	m_nWrite.store( 0, std::memory_order_relaxed );
	m_nRead.store( 0, std::memory_order_relaxed );
	for ( uint32_t i = 0; i < s_nMaxSinks; ++i ) { m_pSinks[i] = nullptr; m_nInFlight[i].store( 0, std::memory_order_relaxed ); }
	if ( m_Config.bEnableFile ) static_cast<void>( attachSink( m_File ) );
	if ( m_Config.bEnableConsole ) static_cast<void>( attachSink( m_Console ) );
	m_bRunning.store( true, std::memory_order_release );
	m_bAccepting.store( true, std::memory_order_release );
	g_pLogger.store( this, std::memory_order_release );
	if ( !m_Thread.start( &SIRLoggerSystem::worker, this ) ) {
		g_pLogger.store( nullptr, std::memory_order_release ); m_bAccepting.store( false, std::memory_order_release ); m_bRunning.store( false, std::memory_order_release );
		return SIRSystemResult::SIR_SYSTEM_ERROR;
	}
	return SIRSystemResult::SIR_SYSTEM_OK;
}

sirLogSinkResult SIRLoggerSystem::write( const sirLogRecord_t& record ) noexcept
{
	if ( g_bLoggerSinkCallback ) return sirLogSinkResult::SIR_LOG_SINK_ERROR;
	if ( !m_bAccepting.load( std::memory_order_acquire ) ) return sirLogSinkResult::SIR_LOG_SINK_BUSY;
	while ( m_ProducerGate.test_and_set( std::memory_order_acquire ) ) SIRThread::cpuRelax();
	const uint32_t nWrite = m_nWrite.load( std::memory_order_relaxed );
	const uint32_t nRead = m_nRead.load( std::memory_order_acquire );
	if ( nWrite - nRead >= m_Config.nQueueCapacity ) { m_ProducerGate.clear( std::memory_order_release ); return sirLogSinkResult::SIR_LOG_SINK_FULL; }
	m_Slots[nWrite % m_Config.nQueueCapacity].record = record;
	m_nWrite.store( nWrite + 1, std::memory_order_release );
	m_ProducerGate.clear( std::memory_order_release );
	return sirLogSinkResult::SIR_LOG_SINK_OK;
}

sirLogSinkResult SIRLoggerSystem::attachSink( SIRLoggerSink& sink ) noexcept
{
	if ( &sink == this ) return sirLogSinkResult::SIR_LOG_SINK_INVALID;
	for ( uint32_t i = 0; i < s_nMaxSinks; ++i ) if ( m_pSinks[i] == &sink ) return sirLogSinkResult::SIR_LOG_SINK_INVALID;
	for ( uint32_t i = 0; i < s_nMaxSinks; ++i ) if ( m_pSinks[i] == nullptr ) { m_pSinks[i] = &sink; return sirLogSinkResult::SIR_LOG_SINK_OK; }
	return sirLogSinkResult::SIR_LOG_SINK_FULL;
}
sirLogSinkResult SIRLoggerSystem::detachSink( SIRLoggerSink& sink ) noexcept
{
	for ( uint32_t i = 0; i < s_nMaxSinks; ++i ) if ( m_pSinks[i] == &sink ) {
		m_pSinks[i] = nullptr;
		if ( m_nInFlight[i].load( std::memory_order_acquire ) != 0 ) return sirLogSinkResult::SIR_LOG_SINK_BUSY;
		return sirLogSinkResult::SIR_LOG_SINK_OK;
	}
	return sirLogSinkResult::SIR_LOG_SINK_INVALID;
}
void SIRLoggerSystem::deliver( const sirLogRecord_t& record ) noexcept
{
	g_bLoggerSinkCallback = true;
	for ( uint32_t i = 0; i < s_nMaxSinks; ++i ) {
		SIRLoggerSink *pSink = m_pSinks[i];
		if ( pSink == nullptr ) continue;
		m_nInFlight[i].fetch_add( 1, std::memory_order_acq_rel );
		static_cast<void>( pSink->write( record ) );
		m_nInFlight[i].fetch_sub( 1, std::memory_order_acq_rel );
	}
	g_bLoggerSinkCallback = false;
}
void SIRLoggerSystem::worker( void *pContext ) noexcept { static_cast<SIRLoggerSystem *>( pContext )->runWorker(); }
void SIRLoggerSystem::runWorker( void ) noexcept
{
	while ( m_bRunning.load( std::memory_order_acquire ) || m_nRead.load( std::memory_order_acquire ) != m_nWrite.load( std::memory_order_acquire ) ) {
		const uint32_t nRead = m_nRead.load( std::memory_order_relaxed );
		const uint32_t nWrite = m_nWrite.load( std::memory_order_acquire );
		if ( nRead == nWrite ) { SIRThread::yieldThread(); continue; }
		deliver( m_Slots[nRead % m_Config.nQueueCapacity].record );
		m_nRead.store( nRead + 1, std::memory_order_release );
	}
}
sirLogSinkResult SIRLoggerSystem::flush( void ) noexcept
{
	while ( m_nRead.load( std::memory_order_acquire ) != m_nWrite.load( std::memory_order_acquire ) ) SIRThread::yieldThread();
	sirLogSinkResult eResult = sirLogSinkResult::SIR_LOG_SINK_OK;
	for ( uint32_t i = 0; i < s_nMaxSinks; ++i ) if ( m_pSinks[i] != nullptr && m_pSinks[i]->flush() != sirLogSinkResult::SIR_LOG_SINK_OK ) eResult = sirLogSinkResult::SIR_LOG_SINK_ERROR;
	return eResult;
}
void SIRLoggerSystem::update( float ) noexcept {}
SIRSystemResult SIRLoggerSystem::shutdown( void ) noexcept
{
	if ( !isInitialized() ) return SIRSystemResult::SIR_SYSTEM_OK;
	m_bAccepting.store( false, std::memory_order_release );
	g_pLogger.store( nullptr, std::memory_order_release );
	m_bRunning.store( false, std::memory_order_release );
	if ( !m_Thread.join() ) return SIRSystemResult::SIR_SYSTEM_BUSY;
	static_cast<void>( flush() );
	for ( uint32_t i = 0; i < s_nMaxSinks; ++i ) m_pSinks[i] = nullptr;
	return SIRSystemResult::SIR_SYSTEM_OK;
}
sirLogSinkResult sirLogWrite( const sirLogRecord_t& record ) noexcept
{
	SIRLoggerSystem *pLogger = g_pLogger.load( std::memory_order_acquire );
	return pLogger == nullptr ? sirLogSinkResult::SIR_LOG_SINK_BUSY : pLogger->write( record );
}
bool sirLogIsEnabled( sirLogSeverity eSeverity ) noexcept { return eSeverity != sirLogSeverity::SIR_LOG_OFF; }
