/* SIREngine MPLv2 Source Code. Copyright (C) 2023-2027 Noah Van Til.
 * This Source Code Form is subject to the Mozilla Public License, v2.0. */
#include <Core/Platform/Shared/Window/SIRNativeWindowHost.h>
#include <Core/Platform/Windows/Window/SIRWindowDriverWin32.h>

int32_t SIRNativeWindowHost::runNative( const sirWindowCreateDesc_t *pDescs, uint32_t nWindows ) noexcept
{
	SIRWindowDriverWin32 driver;
	return runWithDriver( driver, pDescs, nWindows );
}
