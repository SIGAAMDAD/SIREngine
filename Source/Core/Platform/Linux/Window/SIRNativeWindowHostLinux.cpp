/* SIREngine MPLv2 Source Code. Copyright (C) 2023-2027 Noah Van Til.
 * This Source Code Form is subject to the Mozilla Public License, v2.0. */
#include <Core/Platform/Shared/Window/SIRNativeWindowHost.h>
#if defined( SIR_WINDOW_WAYLAND_AVAILABLE )
#include <Core/Platform/Linux/Window/SIRWindowDriverWayland.h>
#include <stdlib.h>
#endif
#if defined( SIR_WINDOW_X11_AVAILABLE )
#include <Core/Platform/Linux/Window/SIRWindowDriverX11.h>
#endif
#include <stdio.h>

int32_t SIRNativeWindowHost::runNative( const sirWindowCreateDesc_t *pDescs, uint32_t nWindows ) noexcept
{
#if defined( SIR_WINDOW_WAYLAND_AVAILABLE )
	const char *pszDisplay = getenv( "WAYLAND_DISPLAY" );
	if ( pszDisplay != nullptr && pszDisplay[0] != 0 ) {
		SIRWindowDriverWayland driver;
		return runWithDriver( driver, pDescs, nWindows );
	}
#endif
#if defined( SIR_WINDOW_X11_AVAILABLE )
	SIRWindowDriverX11 driver;
	return runWithDriver( driver, pDescs, nWindows );
#elif defined( SIR_WINDOW_WAYLAND_AVAILABLE )
	SIRWindowDriverWayland driver;
	return runWithDriver( driver, pDescs, nWindows );
#else
	static_cast<void>( pDescs );
	static_cast<void>( nWindows );
	fputs( "SIREngine: no native window driver is available in this build\n", stderr );
	return 1;
#endif
}
