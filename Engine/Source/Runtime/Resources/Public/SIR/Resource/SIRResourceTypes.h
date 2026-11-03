#ifndef SIR_RESOURCE_TYPES_H
#define SIR_RESOURCE_TYPES_H

#include <stdint.h>

// These are data-representation categories, not rendering/physics/audio classes.
typedef enum class SIRResourceKind : uint8_t
{
    Blob = 0,
    Image,
    Geometry,
    Field,
    Table,
    Graph,
    Hierarchy,
    TimeSeries,
    SampleStream,
    Composition
} SIRResourceKind;

typedef enum class SIRResourceState : uint8_t
{
    Unloaded = 0,
    Requested,
    Staging,
    Resident,
    Evicting,
    Failed
} SIRResourceState;

typedef enum class SIRResourceResult : uint8_t
{
    Success = 0,
    InvalidArgument,
    InvalidHandle,
    TypeMismatch,
    NotResident,
    MissingChunk,
    DependencyMissing,
    DependencyCycle,
    OutOfBudget,
    InvalidFormat,
    WriterViolation,
    Busy,
    Failed
} SIRResourceResult;

typedef enum class SIRResourceEdgeKind : uint8_t
{
    Required = 0,
    Optional,
    Streamed
} SIRResourceEdgeKind;

// A stable cooked identity. Variant is a cooked platform/config variant, not a second CPU owner.
struct SIRResourceKey
{
    uint64_t m_nAssetId;
    uint32_t m_nTypeId;
    uint32_t m_nVariantId;
};

// Process-local and generation-safe: never serialize this in place of SIRResourceKey.
struct SIRResourceHandle
{
    uint32_t m_nSlot;
    uint32_t m_nGeneration;
    uint32_t m_nTypeId;
    uint32_t m_nReserved;
};

struct SIRResourceChunkId
{
    uint32_t m_nValue;
};

struct SIRResourceByteRange
{
    uint64_t m_nOffset;
    uint64_t m_nSize;
};

// Borrowed memory. Valid only while the relevant read scope is alive.
struct SIRResourceConstBytes
{
    const uint8_t *m_pData;
    uint64_t m_nSize;
};

// Unpublished staging bytes. Never exposed through a published resource view.
struct SIRResourceMutableBytes
{
    uint8_t *m_pData;
    uint64_t m_nSize;
};

struct SIRResourceDependency
{
    SIRResourceKey m_Target;
    SIRResourceEdgeKind m_eKind;
    uint8_t m_szReserved[7];
};

struct SIRResourceInfo
{
    SIRResourceKey m_Key;
    uint64_t m_nRevision;
    uint64_t m_nLogicalBytes;
    uint32_t m_nFormatId;
    uint32_t m_nChunkCount;
    uint32_t m_nDependencyCount;
    SIRResourceKind m_eKind;
    SIRResourceState m_eState;
    uint16_t m_nSchemaVersion;
};

struct SIRResourceContentKey
{
    uint64_t m_nHashLo;
    uint64_t m_nHashHi;
    uint32_t m_nFormatId;
    uint32_t m_nEncodingId;
};

struct SIRResourceChunkInfo
{
    SIRResourceChunkId m_Id;
    uint32_t m_nAlignment;
    uint64_t m_nBytes;
    SIRResourceContentKey m_ContentKey;
};

struct SIRResourcePinHandle
{
    uint32_t m_nSlot;
    uint32_t m_nGeneration;
};

struct SIRResourceStagingHandle
{
    uint32_t m_nSlot;
    uint32_t m_nGeneration;
};

struct SIRResourceChunkPublication
{
    SIRResourceChunkId m_Id;
    SIRResourceContentKey m_ContentKey;
    SIRResourceStagingHandle m_Staging;
    bool m_bReuseExisting; // Attach an existing identical chunk without allocating.
};

struct SIRResourcePublication
{
    SIRResourceKey m_Key;
    SIRResourceKind m_eKind;
    uint16_t m_nSchemaVersion;
    uint32_t m_nFormatId;
    const SIRResourceChunkPublication *m_pChunks;
    uint32_t m_nChunkCount;
    const SIRResourceDependency *m_pDependencies;
    uint32_t m_nDependencyCount;
};

// Opaque provider-native objects are registered with the same resource identity/revision.
struct SIRDerivedResourceKey
{
    SIRResourceHandle m_Resource;
    uint64_t m_nRevision;
    uint32_t m_nProviderId;
    uint32_t m_nRepresentationId;
    uint32_t m_nVariantId;
    uint32_t m_nReserved;
};

typedef enum class SIRMemorySpace : uint8_t
{
    Cpu = 0,
    Device,
    External
} SIRMemorySpace;

struct SIRDerivedResourceAccounting
{
    SIRMemorySpace m_eSpace;
    uint32_t m_nDomainId;
    uint64_t m_nCommittedBytes;
    uint64_t m_nResidentBytes;
    uint64_t m_nSourceRevision;
};

static_assert(sizeof(SIRResourceKey) == 16, "Resource keys must remain compact");
static_assert(sizeof(SIRResourceHandle) == 16, "Resource handles must remain generation-safe");
#endif
