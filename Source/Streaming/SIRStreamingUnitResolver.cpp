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

#include <Streaming/SIRStreamingUnitResolver.h>
#include <string.h>
static bool sirUnitEqual( const sirStreamingUnitKey_t& a, const sirStreamingUnitKey_t& b ) noexcept { return a.nContainerId==b.nContainerId&&a.nUnitId==b.nUnitId&&a.nManifestGeneration==b.nManifestGeneration&&a.nVariant==b.nVariant&&a.eKind==b.eKind; }
sirStreamingResult_t SIRStreamingUnitResolver::initialize(const sirStreamingUnitResolverConfig_t& c) noexcept{if(m_bInitialized||c.pRecords==nullptr||c.nCapacity==0)return SIRStreamingResult::SIR_STREAMING_INVALID_ARGUMENT;m_pRecords=c.pRecords;m_nCapacity=c.nCapacity;m_nCount=0;m_nGeneration=0;m_bInitialized=true;return SIRStreamingResult::SIR_STREAMING_OK;}
sirStreamingResult_t SIRStreamingUnitResolver::replaceManifest(const sirStreamingUnitManifest_t& m) noexcept{if(!m_bInitialized||m.pRecords==nullptr||m.nRecordCount>m_nCapacity||m.nGeneration==0)return SIRStreamingResult::SIR_STREAMING_INVALID_ARGUMENT;for(uint32_t i=0;i<m.nRecordCount;++i)m_pRecords[i]=m.pRecords[i];m_nCount=m.nRecordCount;m_nGeneration=m.nGeneration;return SIRStreamingResult::SIR_STREAMING_OK;}
sirStreamingResult_t SIRStreamingUnitResolver::resolve(const sirStreamingUnitKey_t& k,sirStreamingUnitPlan_t *p) const noexcept{if(!m_bInitialized||p==nullptr||p->pChunks==nullptr)return SIRStreamingResult::SIR_STREAMING_INVALID_ARGUMENT;for(uint32_t i=0;i<m_nCount;++i)if(sirUnitEqual(m_pRecords[i].key,k)){p->nRequired=m_pRecords[i].nChunkCount;if(p->nCapacity<m_pRecords[i].nChunkCount)return SIRStreamingResult::SIR_STREAMING_OUT_OF_CAPACITY;memcpy(p->pChunks,m_pRecords[i].pChunks,sizeof(*p->pChunks)*p->nRequired);return SIRStreamingResult::SIR_STREAMING_OK;}p->nRequired=0;return SIRStreamingResult::SIR_STREAMING_NOT_READY;}
