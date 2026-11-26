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

#ifndef __SIR_LOGMACROS_H__
#define __SIR_LOGMACROS_H__
#include <Core/Diagnostics/Logging/SIRLogger.h>
#include <cstdio>
#ifndef SIR_LOG_COMPILED_MINIMUM
#define SIR_LOG_COMPILED_MINIMUM sirLogSeverity::SIR_LOG_TRACE
#endif
#define SIR_LOG( nCategory, eSeverity, pszFormat, ... ) \
	do { if constexpr ( ( eSeverity ) == sirLogSeverity::SIR_LOG_FATAL || ( eSeverity ) >= SIR_LOG_COMPILED_MINIMUM ) { \
		if ( sirLogIsEnabled( ( eSeverity ) ) ) { sirLogRecord_t r{}; r.nCategory = ( nCategory ); r.eSeverity = ( eSeverity ); r.nSourceLine = __LINE__; \
			r.nMessageBytes = static_cast<uint32_t>( std::snprintf( r.szMessage, sizeof( r.szMessage ), ( pszFormat ), ##__VA_ARGS__ ) ); \
			if ( r.nMessageBytes >= sizeof( r.szMessage ) ) r.nMessageBytes = sizeof( r.szMessage ) - 1; static_cast<void>( sirLogWrite( r ) ); } } } while ( false )
#endif
