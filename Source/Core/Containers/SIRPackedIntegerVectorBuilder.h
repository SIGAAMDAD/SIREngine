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

/* SIREngine MPLv2 - transient builder for frozen packed integer vectors. */
#ifndef __SIR_PACKEDINTEGERVECTORBUILDER_H__
#define __SIR_PACKEDINTEGERVECTORBUILDER_H__
#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on
#include <Core/Containers/SIRPackedIntegerVector.h>

class SIRPackedIntegerVectorBuilder
{
public:
	explicit SIRPackedIntegerVectorBuilder( const sirContainerAllocator_t *pAllocator ) noexcept
	    : m_Values( pAllocator )
	{
	}

	SIRContainerResult tryAppend( uint64_t nValue )
	{
		return m_Values.tryPushBack( nValue );
	}

	uint32_t getSize( void ) const noexcept
	{
		return m_Values.getSize();
	}

	SIRContainerResult tryBuild( uint8_t nBits, SIRPackedIntegerVector *pOutput )
	{
		if ( pOutput == nullptr ) {
			return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
		}
		SIRContainerResult eResult = pOutput->tryInitialize( nBits, m_Values.getSize() );
		if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
			return eResult;
		}
		for ( uint32_t i = 0; i < m_Values.getSize(); ++i ) {
			eResult = pOutput->tryAppend( m_Values[i] );
			if ( eResult != SIRContainerResult::SIR_CONTAINER_OK ) {
				return eResult;
			}
		}
		return pOutput->seal();
	}

private:
	SIRVector<uint64_t> m_Values;
};
#endif // __SIR_PACKEDINTEGERVECTORBUILDER_H__
