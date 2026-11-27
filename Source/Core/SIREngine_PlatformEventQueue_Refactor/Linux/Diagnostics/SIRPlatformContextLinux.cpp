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

#include <Core/Platform/Shared/Diagnostics/SIRPlatformContext.h>
#include <Core/Platform/Shared/Threading/SIRPlatformThreadControl.h>
#include <unistd.h>

bool SIRPlatformContext::capture( sirPlatformContext_t *pContext ) noexcept
{
	if ( pContext == nullptr ) {
		return false;
	}
	const uint64_t nThreadId = SIRPlatformThreadControl::getCurrentThreadId();
	const pid_t nPid = getpid();
	if ( nThreadId == 0 || nPid <= 0 ) {
		return false;
	}
	sirPlatformContext_t context{};
	context.ePlatform = sirPlatformKind::SIR_PLATFORM_LINUX;
	context.nProcessId = static_cast<uint32_t>( nPid );
	context.nThreadId = nThreadId;
	*pContext = context;
	return true;
}
