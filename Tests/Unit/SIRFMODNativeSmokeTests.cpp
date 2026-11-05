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

#include <Audio/FMOD/SIRFMODContext.h>

int main( void )
{
	SIRFMODContext context;
	const sirFMODContextConfig_t config = { 64, -1, 1, 0, 0, 0 };
	const sirFMODNativeOps_t ops = sirGetFMODNativeOps();
	if ( context.initialize( config, ops ) != SIR_FMOD_OK ) {
		return 1;
	}
	if ( context.update() != SIR_FMOD_OK ) {
		context.shutdown();
		return 2;
	}
	if ( context.shutdown() != SIR_FMOD_OK ) {
		return 3;
	}
	if ( context.initialize( config, ops ) != SIR_FMOD_OK ) {
		return 4;
	}
	if ( context.shutdown() != SIR_FMOD_OK ) {
		return 5;
	}
	return 0;
}
