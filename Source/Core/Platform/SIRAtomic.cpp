/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.

This software is provided "as is", without warranty of any kind,
express or implied, including but not limited to the warranties
of merchantability, fitness for a particular purpose and noninfringement.
===========================================================================
*/

#include <Core/Platform/SIRAtomic.h>

SIRAtomicFlag::SIRAtomicFlag( void ) noexcept : m_bValue( false )
{}

bool SIRAtomicFlag::test( sirMemoryOrder_t eOrder ) const noexcept
{
	return m_bValue.load( eOrder );
}

bool SIRAtomicFlag::testAndSet( sirMemoryOrder_t eOrder ) noexcept
{
	return m_bValue.exchange( true, eOrder );
}

void SIRAtomicFlag::clear( sirMemoryOrder_t eOrder ) noexcept
{
	m_bValue.store( false, eOrder );
}

bool SIRAtomicFlag::isLockFree( void ) const noexcept
{
	return m_bValue.isLockFree();
}

