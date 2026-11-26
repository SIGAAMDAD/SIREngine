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

// Copyright (c) 2023-2026 Terrain3D contributors.
#ifndef TERRAIN3D_VT_FEEDBACK_EPOCH_H
#define TERRAIN3D_VT_FEEDBACK_EPOCH_H

#include <stdint.h>

// Render-thread ownership. A partial readback pair must finish draining
// before another pair is submitted; it cannot authorize a buffer reset.
struct t3dVTFeedbackEpochState_t
{
	uint32_t m_nGeneration = 1u;
	bool m_bHashPending = false;
	bool m_bCountersPending = false;
	bool m_bPairAccepted = false;
	bool m_bHashComplete = false;
	bool m_bCountersComplete = false;

	void invalidate( const uint32_t nGeneration )
	{
		m_nGeneration = nGeneration;
		m_bHashPending = false;
		m_bCountersPending = false;
		m_bPairAccepted = false;
		m_bHashComplete = false;
		m_bCountersComplete = false;
	}

	bool canSubmit( void ) const
	{
		return !m_bHashPending && !m_bCountersPending;
	}

	bool acceptSubmissions( const bool bHashAccepted, const bool bCountersAccepted )
	{
		m_bHashPending = bHashAccepted;
		m_bCountersPending = bCountersAccepted;
		m_bPairAccepted = bHashAccepted && bCountersAccepted;
		m_bHashComplete = false;
		m_bCountersComplete = false;
		return m_bPairAccepted;
	}

	bool completeHash( const uint32_t nGeneration )
	{
		if ( nGeneration != m_nGeneration || !m_bHashPending ) {
			return false;
		}
		m_bHashPending = false;
		m_bHashComplete = true;
		return m_bPairAccepted;
	}

	bool completeCounters( const uint32_t nGeneration )
	{
		if ( nGeneration != m_nGeneration || !m_bCountersPending ) {
			return false;
		}
		m_bCountersPending = false;
		m_bCountersComplete = true;
		return m_bPairAccepted;
	}

	bool canConsume( void ) const
	{
		return m_bPairAccepted && m_bHashComplete && m_bCountersComplete;
	}
};

#endif // TERRAIN3D_VT_FEEDBACK_EPOCH_H
