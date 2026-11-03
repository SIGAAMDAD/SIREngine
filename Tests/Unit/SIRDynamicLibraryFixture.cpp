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

#if defined( _WIN32 )
#define SIR_TEST_EXPORT __declspec( dllexport )
#else
#define SIR_TEST_EXPORT __attribute__( ( visibility( "default" ) ) )
#endif

extern "C" SIR_TEST_EXPORT int sirPlatformTestValue( void ) noexcept
{
	return 42;
}
