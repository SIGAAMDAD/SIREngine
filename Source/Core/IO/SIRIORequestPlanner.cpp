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

#include <Core/IO/SIRIORequestPlanner.h>
SIRIORequestPlanner::SIRIORequestPlanner( void ) noexcept:m_pPending(nullptr),m_nCount(0),m_nCapacity(0),m_nNextGeneration(0),m_nBuiltCount(0),m_nMergeGap(0),m_nMaxMerged(0),m_bInitialized(false),m_bBuilt(false){}
sirIOResult_t SIRIORequestPlanner::initialize(const sirIOPlannerConfig_t& c) noexcept{if(m_bInitialized||c.pPending==nullptr||c.nPendingCapacity==0)return SIR_IO_INVALID_ARGUMENT;m_pPending=c.pPending;m_nCapacity=c.nPendingCapacity;m_nMergeGap=c.nMaxMergeGap;m_nMaxMerged=c.nMaxMergedBytes;m_nCount=0;m_bBuilt=false;m_bInitialized=true;return SIR_IO_OK;}
sirIOResult_t SIRIORequestPlanner::enqueue(const sirIOPlanInput_t& i) noexcept{if(!m_bInitialized||i.pSource==nullptr||i.nBytes==0)return SIR_IO_INVALID_ARGUMENT;if(m_nCount==m_nCapacity)return SIR_IO_OUT_OF_CAPACITY;m_pPending[m_nCount++]=i;return SIR_IO_OK;}
sirIOResult_t SIRIORequestPlanner::buildBatch(const sirIOBatchBudget_t& b,sirIOPlannedBatch_t *p) noexcept{if(!m_bInitialized||p==nullptr||p->pRanges==nullptr||p->nCapacity==0||m_bBuilt||b.nBytes==0||b.nRequests==0)return SIR_IO_INVALID_ARGUMENT;uint64_t nBytes=0;uint32_t n=0;for(uint32_t i=0;i<m_nCount&&n<p->nCapacity&&n<b.nRequests;++i){uint64_t nRequest=m_pPending[i].nBytes;if(nRequest>m_nMaxMerged||nRequest>b.nBytes-nBytes)continue;p->pRanges[n++]={m_pPending[i].pSource,m_pPending[i].nOffset,nRequest,m_pPending[i].nWorkId};nBytes+=nRequest;}if(n==0)return SIR_IO_NOT_READY;p->nCount=n;p->handle={0,++m_nNextGeneration};m_nBuiltCount=n;m_bBuilt=true;return SIR_IO_OK;}
sirIOResult_t SIRIORequestPlanner::commitBatch(sirIOHandle_t h) noexcept{if(!m_bBuilt||h.nGeneration!=m_nNextGeneration)return SIR_IO_STALE_HANDLE;if(m_nBuiltCount>m_nCount)return SIR_IO_STALE_HANDLE;for(uint32_t i=m_nBuiltCount;i<m_nCount;++i)m_pPending[i-m_nBuiltCount]=m_pPending[i];m_nCount-=m_nBuiltCount;m_nBuiltCount=0;m_bBuilt=false;return SIR_IO_OK;}
sirIOResult_t SIRIORequestPlanner::discardBatch(sirIOHandle_t h) noexcept{if(!m_bBuilt||h.nGeneration!=m_nNextGeneration)return SIR_IO_STALE_HANDLE;m_nBuiltCount=0;m_bBuilt=false;return SIR_IO_OK;}
void SIRIORequestPlanner::reset(void) noexcept{m_nCount=0;m_bBuilt=false;}
