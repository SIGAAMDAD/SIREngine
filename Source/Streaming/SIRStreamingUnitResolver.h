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

#ifndef __SIR_STREAMINGUNITRESOLVER_H__
#define __SIR_STREAMINGUNITRESOLVER_H__
#include <Streaming/SIRStreamingTypes.h>
#include <Streaming/Pack/SIRPackRuntimeTypes.h>
typedef struct sirStreamingChunkDemand_s { sirPackLookup_t lookup; uint32_t nChunkId; SIRStreamingTarget eTarget; } sirStreamingChunkDemand_t;
typedef struct sirStreamingUnitRecord_s { sirStreamingUnitKey_t key; const sirStreamingChunkDemand_t *pChunks; uint32_t nChunkCount; } sirStreamingUnitRecord_t;
typedef struct sirStreamingUnitManifest_s { uint32_t nGeneration; const sirStreamingUnitRecord_t *pRecords; uint32_t nRecordCount; } sirStreamingUnitManifest_t;
typedef struct sirStreamingUnitResolverConfig_s { sirStreamingUnitRecord_t *pRecords; uint32_t nCapacity; } sirStreamingUnitResolverConfig_t;
typedef struct sirStreamingUnitPlan_s { sirStreamingChunkDemand_t *pChunks; uint32_t nCapacity; uint32_t nRequired; } sirStreamingUnitPlan_t;
class SIRStreamingUnitResolver final
{
public:
	sirStreamingResult_t initialize( const sirStreamingUnitResolverConfig_t& ) noexcept;
	sirStreamingResult_t resolve( const sirStreamingUnitKey_t&, sirStreamingUnitPlan_t* ) const noexcept;
	sirStreamingResult_t replaceManifest( const sirStreamingUnitManifest_t& ) noexcept;
private: sirStreamingUnitRecord_t *m_pRecords = nullptr; uint32_t m_nCount = 0, m_nCapacity = 0, m_nGeneration = 0; bool m_bInitialized = false;
};
#endif
