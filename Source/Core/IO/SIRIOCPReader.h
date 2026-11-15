/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til
This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
file, You can obtain one at https://mozilla.org/MPL/2.0/.
===========================================================================
*/

#ifndef __SIRENGINE_SIRIOCPREADER_H__
#define __SIRENGINE_SIRIOCPREADER_H__

#include <Core/Platform/SIRPlatformFile.h>

// Windows worker-owned IO completion port. The shared asynchronous service
// continues to own all source, staging, pin and completion-ticket lifetimes.
class SIRIOCPReader final
{
private:
	void *m_pCompletionPort;
	bool submitRead( const SIRPlatformFile& file, uint64_t nOffset, uint8_t *pOutput, uint32_t nBytes, uint32_t *pTransferred ) noexcept;

public:
	SIRIOCPReader( void ) noexcept;
	~SIRIOCPReader( void ) noexcept;
	SIRIOCPReader( const SIRIOCPReader& ) = delete;
	SIRIOCPReader& operator=( const SIRIOCPReader& ) = delete;

	bool initialize( void ) noexcept;
	void shutdown( void ) noexcept;
	bool isAvailable( void ) const noexcept;
	sirFileResult_t readAt( const SIRPlatformFile& file, uint64_t nOffset, void *pOutput, uint64_t nBytes,
	                        uint64_t *pTransferred ) noexcept;
};

#endif // __SIRENGINE_SIRIOCPREADER_H__
