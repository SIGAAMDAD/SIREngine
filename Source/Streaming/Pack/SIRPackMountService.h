#ifndef __SIR_PACKMOUNTSERVICE_H__
#define __SIR_PACKMOUNTSERVICE_H__

#include <Streaming/Pack/SIRPack.h>
#include <Streaming/Pack/SIRCompressionRegistry.h>
#include <Core/VFS/SIRVFSMountTable.h>

class SIRPackMountService;

class SIRPackSourceLease final
{
public:
	SIRPackSourceLease( void ) noexcept;
	~SIRPackSourceLease( void ) noexcept;
	SIRPackSourceLease( const SIRPackSourceLease& ) = delete;
	SIRPackSourceLease& operator=( const SIRPackSourceLease& ) = delete;
	SIRPackSourceLease( SIRPackSourceLease&& other ) noexcept;
	SIRPackSourceLease& operator=( SIRPackSourceLease&& other ) noexcept;

	bool isValid( void ) const noexcept;
	bool hasChunk( void ) const noexcept;
	const SIRPack *getPack( void ) const noexcept;
	const sirPackChunkSource_t& getSource( void ) const noexcept;
	void reset( void ) noexcept;

private:
	friend class SIRPackMountService;
	SIRPackMountService *m_pOwner;
	SIRPack *m_pPack;
	sirVFSMountLease_t m_Lease;
	sirVFSPath_t m_NameSpace;
	sirPackChunkSource_t m_Source;
	bool m_bHasChunk;
};

typedef struct sirPackMountConfig_s
{
	SIRVFSMountTable *pVfs;
	SIRPack *pPackageStorage;
	const SIRCompressionRegistry *pCodecRegistry;
	uint32_t nPackageCapacity;
	void *pRecordStorage;
	uint64_t nRecordStorageBytes;
} sirPackMountConfig_t;

typedef struct sirPackMountDesc_s
{
	sirVFSMountDescriptor_t vfs;
	SIRPack *pPackage;
	sirPackOpenDesc_t package;
} sirPackMountDesc_t;

class SIRPackMountService final
{
public:
	friend class SIRPackSourceLease;
	SIRPackMountService( void ) noexcept;
	~SIRPackMountService( void ) noexcept;
	SIRPackMountService( const SIRPackMountService& ) = delete;
	SIRPackMountService& operator=( const SIRPackMountService& ) = delete;
	static uint64_t requiredBytes( uint32_t nPackageCapacity ) noexcept;

	sirPackRuntimeResult_t initialize( const sirPackMountConfig_t& config ) noexcept;
	sirPackRuntimeResult_t mount( const sirPackMountDesc_t& desc, sirVFSMountHandle_t *pHandle ) noexcept;
	sirPackRuntimeResult_t unmount( sirVFSMountHandle_t handle ) noexcept;
	sirPackRuntimeResult_t resolveResource( const sirPackLookup_t& lookup, sirPackResource_t *pResource, SIRPackSourceLease *pLease ) noexcept;
	sirPackRuntimeResult_t resolveChunk( const sirPackLookup_t& lookup, uint32_t nChunkId, SIRPackSourceLease *pLease ) noexcept;
	bool isCurrent( const SIRPackSourceLease& lease ) const noexcept;
	uint32_t collectRetired( uint32_t nMaximum ) noexcept;
	sirPackRuntimeResult_t shutdown( void ) noexcept;

private:
	void releaseLease( SIRPackSourceLease& lease ) noexcept;

	public:
	struct MountRecord;
	private:
	SIRVFSMountTable *m_pVfs;
	const SIRCompressionRegistry *m_pCodecRegistry;
	MountRecord *m_pRecords;
	uint32_t m_nCapacity;
	bool m_bInitialized;
};

#endif // __SIR_PACKMOUNTSERVICE_H__
