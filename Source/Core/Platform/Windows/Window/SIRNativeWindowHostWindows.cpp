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

#include <Core/Platform/Shared/Window/SIRNativeWindowHost.h>
#include <Core/Platform/Windows/Window/SIRWindowDriverWin32.h>

int32_t SIRNativeWindowHost::runNative( const sirWindowCreateDesc_t *pDescs, uint32_t nWindows ) noexcept
{
	return runNativeWithFrame( pDescs, nWindows, nullptr, nullptr );
}

int32_t SIRNativeWindowHost::runNativeWithFrame( const sirWindowCreateDesc_t *pDescs, uint32_t nWindows,
	                                               sirNativeWindowFrameFn_t pFrame, void *pContext ) noexcept
{
	SIRWindowDriverWin32 driver;
	return runWithDriverFrame( driver, pDescs, nWindows, pFrame, pContext );
}
