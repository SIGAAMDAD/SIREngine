/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef __SIRENGINE_SIRPACKSOURCEMOUNT_H__
#define __SIRENGINE_SIRPACKSOURCEMOUNT_H__

#include <Core/IO/SIRAsyncIOService.h>
#include <Resources/SIRPackCatalog.h>

// One source generation, bound to the exact already-open native handles.
// Caller retains catalog bytes and validated shard paths until retirement;
// mount/unmount must be driven by the one streaming I/O owner.
class SIRPackSourceMount final
{
private:
	SIRAsyncIOService *m_pIO;
	const SIRPackCatalog *m_pCatalog;
	sirIOSourceHandle_t *m_pHandles;
	uint32_t m_nHandles;
	uint64_t m_nGeneration;

public:
	SIRPackSourceMount( void ) noexcept;
	sirPackResult_t mount( SIRAsyncIOService& io, const SIRPackCatalog& catalog, const char *const *pShardPaths,
	                       sirIOSourceHandle_t *pHandles, uint32_t nCapacity ) noexcept;
	// Refuse unmount if any physical read still retains a source handle.
	sirPackResult_t unmount( void ) noexcept;
	bool isMounted( void ) const noexcept;
	const sirIOSourceHandle_t *getSources( void ) const noexcept;
	uint32_t getSourceCount( void ) const noexcept;
};

#endif // __SIRENGINE_SIRPACKSOURCEMOUNT_H__
