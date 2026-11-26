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

#include <Core/Console/SIRConsoleLogSink.h>

bool SIRConsoleLogSink::initialize( uint32_t nCapacity ) noexcept
{
	m_nCapacity = nCapacity > s_nMaxRecords ? s_nMaxRecords : nCapacity;
	reset();
	return m_nCapacity != 0;
}

sirLogSinkResult SIRConsoleLogSink::write( const sirLogRecord_t& record ) noexcept
{
	const uint32_t nWrite = m_nWrite.load( std::memory_order_relaxed );
	const uint32_t nRead = m_nRead.load( std::memory_order_acquire );
	if ( m_nCapacity == 0 || nWrite - nRead >= m_nCapacity )
		return sirLogSinkResult::SIR_LOG_SINK_FULL;
	m_Records[nWrite % m_nCapacity] = record;
	m_nWrite.store( nWrite + 1, std::memory_order_release );
	return sirLogSinkResult::SIR_LOG_SINK_OK;
}

sirLogSinkResult SIRConsoleLogSink::flush( void ) noexcept
{
	return sirLogSinkResult::SIR_LOG_SINK_OK;
}

uint32_t SIRConsoleLogSink::drain( sirLogRecord_t *pRecords, uint32_t nCapacity ) noexcept
{
	if ( pRecords == nullptr )
		return 0;
	const uint32_t nRead = m_nRead.load( std::memory_order_relaxed );
	const uint32_t nWrite = m_nWrite.load( std::memory_order_acquire );
	const uint32_t nCount = ( nWrite - nRead ) < nCapacity ? ( nWrite - nRead ) : nCapacity;
	for ( uint32_t i = 0; i < nCount; ++i )
		pRecords[i] = m_Records[( nRead + i ) % m_nCapacity];
	m_nRead.store( nRead + nCount, std::memory_order_release );
	return nCount;
}

void SIRConsoleLogSink::reset( void ) noexcept
{
	m_nRead.store( 0, std::memory_order_relaxed );
	m_nWrite.store( 0, std::memory_order_relaxed );
}
