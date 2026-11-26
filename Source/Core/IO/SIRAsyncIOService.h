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

#ifndef __SIR_ASYNC_IO_SERVICE_H__
#define __SIR_ASYNC_IO_SERVICE_H__

#include <Core/SIRPCH.h>

// clang-format off
#ifdef SIR_PRAGMA_ONCE_SUPPORTED
	#pragma once
#endif
// clang-format on

#include <Core/IO/SIRIOTypes.h>

class SIRAsyncIOService
{
public:
	virtual ~SIRAsyncIOService() noexcept = default;

	virtual sirIOResult_t submit( const sirIORequest_t&, sirIOHandle_t * ) noexcept = 0;
	virtual sirIOResult_t submitBatch( const sirIOBatch_t&, sirIOHandle_t * ) noexcept = 0;
	virtual sirIOResult_t cancel( sirIOHandle_t ) noexcept = 0;
	virtual uint32_t collect( sirIOCompletion_t *, uint32_t ) noexcept = 0;
	virtual sirIOResult_t shutdown( void ) noexcept = 0;
};

#endif // __SIR_ASYNC_IO_SERVICE_H__
