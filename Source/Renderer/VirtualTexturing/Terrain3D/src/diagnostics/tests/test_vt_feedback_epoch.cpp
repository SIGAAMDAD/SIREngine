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

// Portable feedback-epoch recovery regression for the P09 publication contract.
#include "rendering/rd/vt/gpu/terrain_3d_vt_feedback_epoch.h"

#include <assert.h>
#include <stdint.h>

int main()
{
	for ( uint32_t mask = 0u; mask < 4u; ++mask ) {
		t3dVTFeedbackEpochState_t state;
		const bool hashAccepted = ( mask & 1u ) != 0u;
		const bool counterAccepted = ( mask & 2u ) != 0u;
		assert( state.acceptSubmissions( hashAccepted, counterAccepted ) == ( hashAccepted && counterAccepted ) );
		assert( !state.canConsume() );
		assert( state.canSubmit() == ( !hashAccepted && !counterAccepted ) );

		if ( hashAccepted ) {
			assert( state.completeHash( 1u ) == ( hashAccepted && counterAccepted ) );
		}
		if ( counterAccepted ) {
			assert( state.completeCounters( 1u ) == ( hashAccepted && counterAccepted ) );
		}
		assert( state.canSubmit() );
		assert( state.canConsume() == ( hashAccepted && counterAccepted ) );
		assert( !state.completeHash( 1u ) );
		assert( !state.completeCounters( 1u ) );

		assert( state.acceptSubmissions( true, true ) );
		assert( state.completeHash( 1u ) );
		state.invalidate( 2u );
		assert( !state.completeCounters( 1u ) );
		assert( state.canSubmit() );
		assert( !state.canConsume() );
		assert( state.acceptSubmissions( true, true ) );
		assert( !state.completeHash( 1u ) );
		assert( state.completeCounters( 2u ) );
		assert( state.completeHash( 2u ) );
		assert( state.canConsume() );
	}
	return 0;
}
