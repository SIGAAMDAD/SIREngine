/* SIREngine MPLv2 Source Code */
#ifndef __SIR_PACKSHARD_H__
#define __SIR_PACKSHARD_H__

#include <Core/Platform/Shared/File/SIRPlatformFile.h>
#include <Streaming/Pack/SIRPackFormat.h>

class SIRPlatformRootDirectory;
typedef struct sirPackShardOpenDesc_s
{
	SIRPlatformRootDirectory *pRoot;
	const char *pszPath;
	sirPackShard_t expected;
	uint64_t nPackageId;
	uint64_t nGeneration;
	void *pValidationScratch;
	uint64_t nValidationScratchBytes;
} sirPackShardOpenDesc_t;

class SIRPackShard final
{
public:
	SIRPackShard( void ) noexcept;
	~SIRPackShard( void ) noexcept;
	SIRPackShard( const SIRPackShard& ) = delete;
	SIRPackShard& operator=( const SIRPackShard& ) = delete;

	bool isOpen( void ) const noexcept;
	uint32_t getIndex( void ) const noexcept;
	uint64_t getSize( void ) const noexcept;

	SIRPackResult open( const sirPackShardOpenDesc_t& desc ) noexcept;
	SIRPackResult close( void ) noexcept;
	SIRPackResult readAt( uint64_t nOffset, void *pDestination, uint64_t nBytes, uint64_t *pTransferred ) const noexcept;
	SIRPackResult validateRange( uint64_t nOffset, uint64_t nBytes ) const noexcept;

private:
	SIRPlatformFile m_File;
	uint64_t m_nSize;
	uint32_t m_iShard;
	bool m_bOpen;
};

#endif // __SIR_PACKSHARD_H__
