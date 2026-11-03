#ifndef SIR_RESOURCE_GRAPH_H
#define SIR_RESOURCE_GRAPH_H

#include "SIRResourceViews.h"

namespace sir
{
class SIRResourceGraphWriter;

// The resource graph owns identities, dependency edges, resident payloads,
// version publication, and deferred reclamation. Readers borrow immutable bytes.
class SIRResourceGraph
{
public:
    SIRResourceGraph(void) noexcept;
    ~SIRResourceGraph(void) noexcept;

    SIRResourceGraph(const SIRResourceGraph &) = delete;
    SIRResourceGraph &operator=(const SIRResourceGraph &) = delete;

    SIRResourceResult find(const SIRResourceReadScope &rScope,
                           const SIRResourceKey &rKey,
                           SIRResourceHandle *pHandle) const noexcept;
    SIRResourceResult resolve(const SIRResourceReadScope &rScope,
                              SIRResourceHandle hResource,
                              SIRResourceView *pView) const noexcept;
    SIRResourceResult resolveDependency(const SIRResourceReadScope &rScope,
                                        const SIRResourceView &rParent,
                                        uint32_t iDependency,
                                        SIRResourceView *pChild) const noexcept;

    // Long-lived provider use is pinned to the exact published revision.
    // Acquiring/releasing a pin updates retention metadata, never payload bytes.
    SIRResourceResult pinForProvider(const SIRResourceReadScope &rScope,
                                     const SIRResourceView &rView,
                                     uint32_t nProviderId,
                                     SIRResourcePinHandle *pPin) const noexcept;
    SIRResourceResult releaseProviderPin(SIRResourcePinHandle hPin) const noexcept;

    // Only the registered owner thread receives the writer capability.
    SIRResourceGraphWriter *getWriter(void) noexcept;

    friend class SIRResourceReadScope;
};

// Staging is a GRDG-owned allocation; publishing transfers its ownership,
// rather than copying its bytes into a consumer-owned buffer.
class SIRResourceGraphWriter
{
private:
    SIRResourceGraph *m_pGraph;
    explicit SIRResourceGraphWriter(SIRResourceGraph &rGraph) noexcept;

public:
    SIRResourceGraphWriter(const SIRResourceGraphWriter &) = delete;
    SIRResourceGraphWriter &operator=(const SIRResourceGraphWriter &) = delete;

    SIRResourceResult allocateStaging(uint64_t nSize,
                                      uint32_t nAlignment,
                                      uint32_t nDomainId,
                                      SIRResourceStagingHandle *pHandle,
                                      SIRResourceMutableBytes *pBytes) noexcept;
    SIRResourceResult cancelStaging(SIRResourceStagingHandle hStaging) noexcept;
    SIRResourceResult request(const SIRResourceKey &rKey, SIRResourceHandle *pHandle) noexcept;
    SIRResourceResult publish(const SIRResourcePublication &rPublication,
                              SIRResourceHandle *pHandle) noexcept;
    SIRResourceResult markFailed(SIRResourceHandle hResource,
                                 SIRResourceResult eCause) noexcept;
    SIRResourceResult evictChunk(SIRResourceHandle hResource,
                                 SIRResourceChunkId id) noexcept;
    SIRResourceResult retire(SIRResourceHandle hResource) noexcept;
    uint32_t collectRetired(uint32_t nMaxRecords) noexcept;

    friend class SIRResourceGraph;
};

// Provider representations can have device/native memory. They are registered
// under canonical identity and revision, with centralized accounting and fences.
struct SIRDerivedResourceHandle
{
    uint32_t m_nSlot;
    uint32_t m_nGeneration;
};

struct SIRDerivedResourceRegistration
{
    SIRDerivedResourceKey m_Key;
    SIRDerivedResourceAccounting m_Accounting;
    uint64_t m_nNativeToken;
};

class SIRResourceDerivedRegistry
{
public:
    // Owner-writer operations. Workers submit completion messages; they do not mutate this registry.
    SIRResourceResult registerRepresentation(const SIRDerivedResourceRegistration &rRegistration,
                                             SIRDerivedResourceHandle *pHandle) noexcept;
    SIRResourceResult retireRepresentation(SIRDerivedResourceHandle hRepresentation,
                                           uint64_t nCompletionFence) noexcept;
    SIRResourceResult updateCompletedFence(uint32_t nProviderId,
                                           uint64_t nCompletedFence) noexcept;
    SIRResourceResult getAccounting(SIRDerivedResourceHandle hRepresentation,
                                    SIRDerivedResourceAccounting *pAccounting) const noexcept;
};
}

#endif
