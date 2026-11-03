#ifndef SIR_RESOURCE_VIEWS_H
#define SIR_RESOURCE_VIEWS_H

#include "SIRResourceTypes.h"

class SIRResourceGraph;
struct SIRPublishedResourceRecord;

// The only reader lifetime root: individual views do not own or retain payloads.
class SIRResourceReadScope
{
private:
    const SIRResourceGraph *m_pGraph;
    uint64_t m_nReaderToken;

public:
    explicit SIRResourceReadScope(const SIRResourceGraph &rGraph) noexcept;
    ~SIRResourceReadScope(void) noexcept;

    SIRResourceReadScope(const SIRResourceReadScope &) = delete;
    SIRResourceReadScope &operator=(const SIRResourceReadScope &) = delete;
    SIRResourceReadScope(SIRResourceReadScope &&) = delete;
    SIRResourceReadScope &operator=(SIRResourceReadScope &&) = delete;

    bool isActive(void) const noexcept;

    friend class SIRResourceGraph;
};

// Lightweight, non-owning read-only projection: exactly two borrowed pointers.
class SIRResourceView
{
protected:
    const SIRResourceReadScope *m_pScope;
    const SIRPublishedResourceRecord *m_pRecord;

    SIRResourceView(const SIRResourceReadScope *pScope, const SIRPublishedResourceRecord *pRecord) noexcept;

public:
    SIRResourceView(void) noexcept;
    bool isValid(void) const noexcept;
    SIRResourceKind getKind(void) const noexcept;
    SIRResourceHandle getHandle(void) const noexcept;
    uint64_t getRevision(void) const noexcept;
    SIRResourceResult getInfo(SIRResourceInfo *pInfo) const noexcept;
    SIRResourceResult getChunk(SIRResourceChunkId id, SIRResourceConstBytes *pBytes) const noexcept;
    SIRResourceResult getChunkInfo(uint32_t iChunk, SIRResourceChunkInfo *pInfo) const noexcept;
    SIRResourceResult getDependency(uint32_t iEdge, SIRResourceDependency *pEdge) const noexcept;

    friend class SIRResourceGraph;
};

// The following descriptors carry no resource bytes or ownership.
struct SIRImageDesc
{
    uint32_t m_nWidth;
    uint32_t m_nHeight;
    uint32_t m_nDepth;
    uint32_t m_nLayers;
    uint32_t m_nMips;
    uint32_t m_nFormatId;
};

struct SIRGeometryDesc
{
    uint32_t m_nVertexCount;
    uint32_t m_nIndexCount;
    uint16_t m_nLodCount;
    uint16_t m_nSubmeshCount;
    uint32_t m_nFormatId;
};

struct SIRFieldDesc
{
    uint32_t m_nWidth;
    uint32_t m_nHeight;
    uint32_t m_nDepth;
    uint16_t m_nMipCount;
    uint16_t m_nChannelCount;
    uint32_t m_nFormatId;
};

struct SIRStreamDesc
{
    uint64_t m_nSampleCount;
    uint32_t m_nSampleRate;
    uint32_t m_nChannelCount;
    uint32_t m_nEncodingId;
};

struct SIRTableDesc
{
    uint64_t m_nRowCount;
    uint32_t m_nColumnCount;
    uint32_t m_nSchemaId;
};

struct SIRGraphDesc
{
    uint32_t m_nNodeCount;
    uint32_t m_nEdgeCount;
    uint32_t m_nSchemaId;
};

struct SIRHierarchyDesc
{
    uint32_t m_nNodeCount;
    uint32_t m_nSchemaId;
};

struct SIRTimeSeriesDesc
{
    uint32_t m_nTrackCount;
    uint32_t m_nTickRate;
    uint64_t m_nDurationTicks;
};

// All subclasses add methods only, never payload storage or owning containers.
class SIRBlobResourceView : public SIRResourceView
{
public:
    static constexpr SIRResourceKind RESOURCE_KIND = SIRResourceKind::Blob;
    explicit SIRBlobResourceView(const SIRResourceView &rView) noexcept : SIRResourceView(rView) {}
    SIRResourceResult getBytes(SIRResourceChunkId id, SIRResourceByteRange range, SIRResourceConstBytes *pBytes) const noexcept;
};

class SIRImageResourceView : public SIRResourceView
{
public:
    static constexpr SIRResourceKind RESOURCE_KIND = SIRResourceKind::Image;
    explicit SIRImageResourceView(const SIRResourceView &rView) noexcept : SIRResourceView(rView) {}
    SIRResourceResult describeImage(SIRImageDesc *pDesc) const noexcept;
    SIRResourceResult getImagePlane(uint32_t iMip, uint32_t iLayer, uint32_t iPlane, SIRResourceConstBytes *pBytes) const noexcept;
};

class SIRGeometryResourceView : public SIRResourceView
{
public:
    static constexpr SIRResourceKind RESOURCE_KIND = SIRResourceKind::Geometry;
    explicit SIRGeometryResourceView(const SIRResourceView &rView) noexcept : SIRResourceView(rView) {}
    SIRResourceResult describeGeometry(SIRGeometryDesc *pDesc) const noexcept;
    SIRResourceResult getVertexStream(uint32_t iLod, uint32_t nSemanticId, SIRResourceConstBytes *pBytes) const noexcept;
    SIRResourceResult getIndexStream(uint32_t iLod, SIRResourceConstBytes *pBytes) const noexcept;
    SIRResourceResult getSubmesh(uint32_t iLod, uint32_t iSubmesh, SIRResourceByteRange *pIndexRange) const noexcept;
};

class SIRFieldResourceView : public SIRResourceView
{
public:
    static constexpr SIRResourceKind RESOURCE_KIND = SIRResourceKind::Field;
    explicit SIRFieldResourceView(const SIRResourceView &rView) noexcept : SIRResourceView(rView) {}
    SIRResourceResult describeField(SIRFieldDesc *pDesc) const noexcept;
    SIRResourceResult getFieldTile(uint32_t iMip, uint32_t nTileId, uint32_t iChannel, SIRResourceConstBytes *pBytes) const noexcept;
};

class SIRTableResourceView : public SIRResourceView
{
public:
    static constexpr SIRResourceKind RESOURCE_KIND = SIRResourceKind::Table;
    explicit SIRTableResourceView(const SIRResourceView &rView) noexcept : SIRResourceView(rView) {}
    SIRResourceResult describeTable(SIRTableDesc *pDesc) const noexcept;
    SIRResourceResult getColumn(uint32_t iColumn, SIRResourceConstBytes *pBytes) const noexcept;
};

class SIRGraphResourceView : public SIRResourceView
{
public:
    static constexpr SIRResourceKind RESOURCE_KIND = SIRResourceKind::Graph;
    explicit SIRGraphResourceView(const SIRResourceView &rView) noexcept : SIRResourceView(rView) {}
    SIRResourceResult describeGraph(SIRGraphDesc *pDesc) const noexcept;
    SIRResourceResult getNodes(SIRResourceConstBytes *pNodes) const noexcept;
    SIRResourceResult getEdges(SIRResourceConstBytes *pEdges) const noexcept;
};

class SIRHierarchyResourceView : public SIRResourceView
{
public:
    static constexpr SIRResourceKind RESOURCE_KIND = SIRResourceKind::Hierarchy;
    explicit SIRHierarchyResourceView(const SIRResourceView &rView) noexcept : SIRResourceView(rView) {}
    SIRResourceResult describeHierarchy(SIRHierarchyDesc *pDesc) const noexcept;
    SIRResourceResult getParentIndices(SIRResourceConstBytes *pIndices) const noexcept;
    SIRResourceResult getLocalTransforms(SIRResourceConstBytes *pTransforms) const noexcept;
};

class SIRTimeSeriesResourceView : public SIRResourceView
{
public:
    static constexpr SIRResourceKind RESOURCE_KIND = SIRResourceKind::TimeSeries;
    explicit SIRTimeSeriesResourceView(const SIRResourceView &rView) noexcept : SIRResourceView(rView) {}
    SIRResourceResult describeTimeSeries(SIRTimeSeriesDesc *pDesc) const noexcept;
    SIRResourceResult getTrack(uint32_t iTrack, SIRResourceConstBytes *pTrack) const noexcept;
};

class SIRSampleStreamResourceView : public SIRResourceView
{
public:
    static constexpr SIRResourceKind RESOURCE_KIND = SIRResourceKind::SampleStream;
    explicit SIRSampleStreamResourceView(const SIRResourceView &rView) noexcept : SIRResourceView(rView) {}
    SIRResourceResult describeStream(SIRStreamDesc *pDesc) const noexcept;
    SIRResourceResult getSampleBlock(uint32_t nBlockId, SIRResourceConstBytes *pBytes) const noexcept;
};

class SIRCompositionResourceView : public SIRResourceView
{
public:
    static constexpr SIRResourceKind RESOURCE_KIND = SIRResourceKind::Composition;
    explicit SIRCompositionResourceView(const SIRResourceView &rView) noexcept : SIRResourceView(rView) {}
    SIRResourceResult getChild(uint32_t iChild, SIRResourceDependency *pChild) const noexcept;
};

// Checked typed conversion: never reinterpret one canonical payload as an unrelated family.
template<class T>
SIRResourceResult tryCastResourceView(const SIRResourceView &rView, T *pOut) noexcept
{
    if (pOut == nullptr)
    {
        return SIRResourceResult::InvalidArgument;
    }
    if (!rView.isValid())
    {
        return SIRResourceResult::InvalidHandle;
    }
    if (rView.getKind() != T::RESOURCE_KIND)
    {
        return SIRResourceResult::TypeMismatch;
    }
    *pOut = T(rView);
    return SIRResourceResult::Success;
}
#endif
