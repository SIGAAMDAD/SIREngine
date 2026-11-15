/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef __SIRENGINE_SIRIOURINGREADER_H__
#define __SIRENGINE_SIRIOURINGREADER_H__

#include <Core/Platform/SIRPlatformFile.h>

// Linux-only, one-worker, kernel-ring positional I/O executor. The shared
// SIRAsyncIOService remains the sole owner of request slots, pins, priorities
// and terminal acknowledgments. This object owns only native kernel resources.
class SIRIOUringReader final
{
private:
	int m_nRing;
	void *m_pSQMapping;
	void *m_pCQMapping;
	void *m_pEntriesMapping;
	uint64_t m_nSQMapBytes;
	uint64_t m_nCQMapBytes;
	uint64_t m_nEntriesMapBytes;
	bool m_bSingleMapping;
	uint32_t *m_pSQHead;
	uint32_t *m_pSQTail;
	uint32_t *m_pSQMask;
	uint32_t *m_pSQArray;
	uint32_t *m_pCQHead;
	uint32_t *m_pCQTail;
	uint32_t *m_pCQMask;
	void *m_pSQEntries;
	void *m_pCQEntries;

	bool submitRead( int nFile, uint64_t nOffset, uint8_t *pOutput, uint32_t nBytes, int32_t *pResult ) noexcept;

public:
	SIRIOUringReader( void ) noexcept;
	~SIRIOUringReader( void ) noexcept;
	SIRIOUringReader( const SIRIOUringReader& ) = delete;
	SIRIOUringReader& operator=( const SIRIOUringReader& ) = delete;

	// No ring allocation is mandatory: EPERM/ENOSYS/ENOMEM transparently
	// selects the production bounded pread worker already in use.
	bool initialize( void ) noexcept;
	void shutdown( void ) noexcept;
	bool isAvailable( void ) const noexcept;
	sirFileResult_t readAt( const SIRPlatformFile& file, uint64_t nOffset, void *pOutput, uint64_t nBytes,
	                        uint64_t *pTransferred ) noexcept;
};

#endif // __SIRENGINE_SIRIOURINGREADER_H__
