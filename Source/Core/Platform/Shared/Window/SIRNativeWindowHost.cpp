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

#include <Core/Platform/Shared/Window/SIRNativeWindowHost.h>
#include <Core/SIRApplication.h>
#include <Core/Platform/Shared/Window/SIRWindowSystem.h>
#include <stdlib.h>
#include <stdio.h>

static SIRAppWindowPumpResult sirNativeWindowHostPump( void *pContext )
{
	SIRWindowSystem *pWindows = static_cast<SIRWindowSystem *>(pContext);
	const sirWindowResult eResult = pWindows->pumpEvents( 256 );
	return eResult == sirWindowResult::SIR_WINDOW_OK || eResult == sirWindowResult::SIR_WINDOW_EVENTS_LOST ? SIRAppWindowPumpResult::SIR_APP_WINDOW_CONTINUE : SIRAppWindowPumpResult::SIR_APP_WINDOW_ERROR;
}

int32_t SIRNativeWindowHost::runWithDriver( SIRWindowDriver& driver, const sirWindowCreateDesc_t *pDescs, uint32_t nWindows ) noexcept
{
	if ( nWindows > SIR_WINDOW_MAX_WINDOWS || ( nWindows != 0 && pDescs == nullptr ) ) {
		return 1;
	}
	SIRWindowSystem windows( driver );
	SIRApplication app;
	if ( !app.registerSystem( &windows ) || !app.attachWindowPump( sirNativeWindowHostPump, &windows ) || !app.initialize() ) {
		static_cast<void>(app.shutdown());
		return 1;
	}
	sirWindowHandle_t handles[SIR_WINDOW_MAX_WINDOWS];
	memset( handles, 0, sizeof( handles ) );
	uint32_t nCreated = 0;
	bool bSucceeded = true;
	for ( uint32_t i = 0; i < nWindows; ++i ) {
		if ( windows.createWindow( pDescs[i], &handles[i] ) != sirWindowResult::SIR_WINDOW_OK ) {
			bSucceeded = false;
			break;
		}
		++nCreated;
	}
	uint32_t nOpen = nCreated;
	while ( bSucceeded && nOpen != 0 ) {
		if ( !app.tick() ) {
			bSucceeded = false;
			break;
		}
		for ( uint32_t i = 0; i < nCreated; ++i ) {
			if ( handles[i].nGeneration == 0 ) {
				continue;
			}
			sirWindowSnapshot_t snapshot {};
			if ( windows.querySnapshot( handles[i], &snapshot ) != sirWindowResult::SIR_WINDOW_OK ) {
				bSucceeded = false;
				break;
			}
			if ( snapshot.bCloseRequested ) {
				if ( windows.destroyWindow( handles[i] ) != sirWindowResult::SIR_WINDOW_OK ) {
					bSucceeded = false;
					break;
				}
				handles[i] = {};
				--nOpen;
			}
		}
	}
	while ( nCreated != 0 ) {
		--nCreated;
		if ( handles[nCreated].nGeneration != 0 && windows.destroyWindow( handles[nCreated] ) != sirWindowResult::SIR_WINDOW_OK ) {
			bSucceeded = false;
		}
	}
	const SIRSystemResult eShutdown = app.shutdown();
	return bSucceeded && eShutdown == SIRSystemResult::SIR_SYSTEM_OK ? 0 : 1;
}

