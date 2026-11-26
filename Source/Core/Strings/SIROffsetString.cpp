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

#include <Core/Strings/SIROffsetString.h>
#include <Core/Strings/SIRStringOps.h>

SIRContainerResult SIROffsetString::tryResolve( sirStringView_t table, sirStringView_t& view ) const noexcept
{
	if ( !SIRStringOps::isValid( table ) || m_iOffset > table.nBytes ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	if ( m_nBytes > table.nBytes - m_iOffset ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	view = { table.pBytes == nullptr ? nullptr : table.pBytes + m_iOffset, m_nBytes };
	return SIRContainerResult::SIR_CONTAINER_OK;
}

SIRContainerResult SIROffsetString::tryFromBoundaries( uint32_t iBegin, uint32_t iEnd, SIROffsetString& output ) noexcept
{
	if ( iEnd < iBegin ) {
		return SIRContainerResult::SIR_CONTAINER_INVALID_ARGUMENT;
	}
	output = SIROffsetString( iBegin, iEnd - iBegin );
	return SIRContainerResult::SIR_CONTAINER_OK;
}
