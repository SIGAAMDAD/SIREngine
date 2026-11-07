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
#include <Core/Platform/SIRPlatformContext.h>
#include <Windows.h>

bool sirPlatformCaptureContext( sirPlatformContext_t *pContext ) noexcept
{
	if ( pContext == nullptr ) {
		return false;
	}
	*pContext = {};
	const DWORD nPid = GetCurrentProcessId();
	const DWORD nTid = GetCurrentThreadId();
	if ( nPid == 0 || nTid == 0 ) {
		return false;
	}
	pContext->nAbiVersion = SIR_PLATFORM_CONTEXT_ABI_VERSION;
	pContext->ePlatform = SIR_PLATFORM_WINDOWS;
	pContext->native.windows.nInstance = ( uintptr_t )GetModuleHandleW( nullptr );
	pContext->native.windows.nProcessId = nPid;
	pContext->native.windows.nOwnerThreadId = nTid;
	return pContext->native.windows.nInstance != 0;
}
