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

#ifndef __SIR_D3_D12_NATIVEACCESS_H__
#define __SIR_D3_D12_NATIVEACCESS_H__

#include <Renderer/Drivers/D3D12/SIRD3D12Device.h>

// The MinGW IDL bindings expose aggregate COM returns through an explicit
// output parameter. Native Windows SDK headers return those aggregates by value.
// Both paths call the same native COM entry points without allocating storage.
static inline D3D12_RESOURCE_DESC sirD3D12GetResourceDesc( ID3D12Resource *pResource ) noexcept
{
#if defined( __MINGW32__ )
	D3D12_RESOURCE_DESC desc {};
	static_cast<void>(pResource->GetDesc( &desc ));
	return desc;
#else
	return pResource->GetDesc();
#endif
}

static inline D3D12_RESOURCE_ALLOCATION_INFO sirD3D12GetResourceAllocationInfo( ID3D12Device *pDevice,
                                                                                const D3D12_RESOURCE_DESC& desc ) noexcept
{
#if defined( __MINGW32__ )
	D3D12_RESOURCE_ALLOCATION_INFO info {};
	static_cast<void>(pDevice->GetResourceAllocationInfo( &info, 0, 1, &desc ));
	return info;
#else
	return pDevice->GetResourceAllocationInfo( 0, 1, &desc );
#endif
}

static inline D3D12_CPU_DESCRIPTOR_HANDLE sirD3D12GetCPUDescriptorStart( ID3D12DescriptorHeap *pHeap ) noexcept
{
#if defined( __MINGW32__ )
	D3D12_CPU_DESCRIPTOR_HANDLE handle {};
	static_cast<void>(pHeap->GetCPUDescriptorHandleForHeapStart( &handle ));
	return handle;
#else
	return pHeap->GetCPUDescriptorHandleForHeapStart();
#endif
}

static inline D3D12_GPU_DESCRIPTOR_HANDLE sirD3D12GetGPUDescriptorStart( ID3D12DescriptorHeap *pHeap ) noexcept
{
#if defined( __MINGW32__ )
	D3D12_GPU_DESCRIPTOR_HANDLE handle {};
	static_cast<void>(pHeap->GetGPUDescriptorHandleForHeapStart( &handle ));
	return handle;
#else
	return pHeap->GetGPUDescriptorHandleForHeapStart();
#endif
}

#endif // __SIR_D3_D12_NATIVEACCESS_H__
