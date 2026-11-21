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

#include <Core/Platform/Window/Windows/SIRWGLBootstrapWindow.h>
#include <stdio.h>

int main( void )
{
	sirWGLBootstrapWindow_t window = {};
	if ( sirCreateWGLBootstrapWindow( nullptr ) || !sirCreateWGLBootstrapWindow( &window ) || window.hWindow == nullptr ||
	     window.hDC == nullptr || !IsWindow( window.hWindow ) || sirCreateWGLBootstrapWindow( &window ) ) {
		fputs( "SIRWGLBootstrapWindowTests create failure\n", stderr );
		return 1;
	}
	sirDestroyWGLBootstrapWindow( &window );
	if ( window.hWindow != nullptr || window.hDC != nullptr ) {
		fputs( "SIRWGLBootstrapWindowTests destroy failure\n", stderr );
		return 1;
	}
	sirDestroyWGLBootstrapWindow( &window );
	sirDestroyWGLBootstrapWindow( nullptr );
	puts( "SIRWGLBootstrapWindowTests PASS" );
	return 0;
}
