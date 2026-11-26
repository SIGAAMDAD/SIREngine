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

#include <Core/Strings/SIRStringBuilder.h>
#include <Core/Strings/SIRStringOps.h>

SIRStringBuilder::SIRStringBuilder( char *pszStorage, uint32_t nCapacity ) noexcept
    : m_pszStorage( pszStorage ),
      m_nCapacity( pszStorage == nullptr ? 0 : nCapacity )
{
	clear();
}

sirStringView_t SIRStringBuilder::getView( void ) const noexcept
{
	return { m_pszStorage, m_nLength };
}

uint32_t SIRStringBuilder::getRemainingCapacity( void ) const noexcept
{
	return m_nCapacity == 0 ? 0 : m_nCapacity - m_nLength - 1;
}

SIRContainerResult SIRStringBuilder::tryAppend( sirStringView_t view ) noexcept
{
	if ( m_nCapacity == 0 ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	const SIRContainerResult eResult = SIRStringOps::copyTerminated( view, m_pszStorage + m_nLength, m_nCapacity - m_nLength );
	if ( eResult == SIRContainerResult::SIR_CONTAINER_OK ) {
		m_nLength += view.nBytes;
	}
	return eResult;
}

SIRContainerResult SIRStringBuilder::tryAppendUnsigned( uint64_t nValue ) noexcept
{
	static constexpr uint64_t nPowers[]{ 1,
	                                     10,
	                                     100,
	                                     1000,
	                                     10000,
	                                     100000,
	                                     1000000,
	                                     10000000,
	                                     100000000,
	                                     1000000000,
	                                     UINT64_C( 10000000000 ),
	                                     UINT64_C( 100000000000 ),
	                                     UINT64_C( 1000000000000 ),
	                                     UINT64_C( 10000000000000 ),
	                                     UINT64_C( 100000000000000 ),
	                                     UINT64_C( 1000000000000000 ),
	                                     UINT64_C( 10000000000000000 ),
	                                     UINT64_C( 100000000000000000 ),
	                                     UINT64_C( 1000000000000000000 ),
	                                     UINT64_C( 10000000000000000000 ) };
	uint32_t iLow = 0;
	uint32_t iHigh = 20;
	while ( iLow + 1 < iHigh ) {
		const uint32_t iMiddle = iLow + ( iHigh - iLow ) / 2;
		if ( nValue >= nPowers[iMiddle] ) {
			iLow = iMiddle;
		} else {
			iHigh = iMiddle;
		}
	}
	const uint32_t nDigits = iLow + 1;
	if ( nDigits > getRemainingCapacity() ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	char *pEnd = m_pszStorage + m_nLength + nDigits;
	static constexpr char szPairs[] = "000102030405060708091011121314151617181920212223242526272829303132333435363738394041424344454647484950515253545556575859"
	                                  "60616263646566676869707172737475767778798081828384858687888990919293949596979899";
	while ( nValue >= 100 ) {
		const uint64_t nQuotient = nValue / 100;
		const uint32_t iPair = static_cast<uint32_t>( nValue - nQuotient * 100 ) * 2;
		pEnd -= 2;
		memcpy( pEnd, szPairs + iPair, 2 );
		nValue = nQuotient;
	}
	if ( nValue >= 10 ) {
		pEnd -= 2;
		memcpy( pEnd, szPairs + nValue * 2, 2 );
	} else {
		*--pEnd = static_cast<char>( '0' + nValue );
	}
	m_nLength += nDigits;
	m_pszStorage[m_nLength] = '\0';
	return SIRContainerResult::SIR_CONTAINER_OK;
}

void SIRStringBuilder::clear( void ) noexcept
{
	m_nLength = 0;
	if ( m_nCapacity != 0 ) {
		m_pszStorage[0] = '\0';
	}
}

SIRContainerResult SIRStringBuilder::tryAppendBatch( const sirStringView_t *pViews, uint32_t nCount ) noexcept
{
	if ( pViews == nullptr && nCount != 0 ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( m_nCapacity == 0 ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	if ( nCount > UINT32_MAX / sizeof( sirStringView_t ) ) {
		return SIRContainerResult::SIR_CONTAINER_SIZE_OVERFLOW;
	}
	uint32_t nTotal = 0;
	const uint32_t nAvailable = getRemainingCapacity();
	for ( uint32_t iView = 0; iView < nCount; ++iView ) {
		if ( !SIRStringOps::isValid( pViews[iView] ) ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		if ( pViews[iView].nBytes > nAvailable - nTotal ) {
			return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
		}
		nTotal += pViews[iView].nBytes;
	}
	const sirStringView_t destination{ m_pszStorage + m_nLength, nTotal + 1 };
	for ( uint32_t iView = 0; iView < nCount; ++iView ) {
		if ( SIRStringOps::overlaps( destination, pViews[iView] ) ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
	}
	if ( SIRStringOps::overlaps( destination, { reinterpret_cast<const char *>( pViews ), static_cast<uint32_t>( nCount * sizeof( sirStringView_t ) ) } ) ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	for ( uint32_t iView = 0; iView < nCount; ++iView ) {
		if ( pViews[iView].nBytes != 0 ) {
			memcpy( m_pszStorage + m_nLength, pViews[iView].pBytes, pViews[iView].nBytes );
		}
		m_nLength += pViews[iView].nBytes;
	}
	m_pszStorage[m_nLength] = '\0';
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIRStringBuilder::tryAppendGenerated( uint32_t nBytes, void ( *pfnWrite )( char *, uint32_t, void * ) noexcept, void *pContext ) noexcept
{
	if ( pfnWrite == nullptr ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( m_nCapacity == 0 || nBytes > getRemainingCapacity() ) {
		return SIRContainerResult::SIR_CONTAINER_CAPACITY_EXCEEDED;
	}
	pfnWrite( m_pszStorage + m_nLength, nBytes, pContext );
	m_nLength += nBytes;
	m_pszStorage[m_nLength] = '\0';
	return SIRContainerResult::SIR_CONTAINER_OK;
}
