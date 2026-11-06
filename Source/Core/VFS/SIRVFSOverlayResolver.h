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

#ifndef __SIRENGINE_SIRVFSOVERLAYRESOLVER_H__
#define __SIRENGINE_SIRVFSOVERLAYRESOLVER_H__

#include <SIRPCH.hpp>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/VFS/SIRVFSMountTable.h>

typedef enum sirVFSProbeResult_e : uint8_t
{
	SIR_VFS_PROBE_PRESENT = 0,
	SIR_VFS_PROBE_MISSING,
	SIR_VFS_PROBE_FORBIDDEN,
	SIR_VFS_PROBE_ERROR
} sirVFSProbeResult_t;

// Must perform a bounded, synchronous metadata lookup, not blocking disk I/O.
// The descriptor and path are borrowed for the duration of this callback only.
// A PRESENT result must denote a resolvable file owned by that mount generation.
typedef sirVFSProbeResult_t ( *sirVFSProbeCallback_t )( const sirVFSMountDescriptor_t *pDescriptor, const sirVFSPath_t *pPath,
                                                        void *pContext ) noexcept;

// Resolves highest-priority visible entry while pinning its source generation.
// The caller owns the returned lease and must release it on the same table.
// The table must outlive this resolver and all acquired leases.
class SIRVFSOverlayResolver final
{
public:
	explicit SIRVFSOverlayResolver( SIRVFSMountTable& table ) noexcept;
	~SIRVFSOverlayResolver( void ) noexcept;

	sirVFSResult_t resolveRead( const sirVFSPath_t& path, sirVFSProbeCallback_t pfnProbe, void *pContext,
	                            sirVFSMountCandidate_t *pWinner ) noexcept;

protected:
private:
	SIRVFSMountTable& m_Table;
};

#endif // __SIRENGINE_SIRVFSOVERLAYRESOLVER_H__
