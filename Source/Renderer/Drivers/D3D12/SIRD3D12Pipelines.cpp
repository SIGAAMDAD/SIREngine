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

#include <Renderer/Drivers/D3D12/SIRD3D12Pipelines.h>
#include <Renderer/Drivers/D3D12/SIRD3D12Validation.h>
#include <Renderer/SIRGraphicsValidation.h>
#include <d3dcompiler.h>
#include <limits.h>
#include <string.h>

static DXGI_FORMAT sirD3D12PipelineFormat( SIRGraphicsFormat eFormat ) noexcept
{
	switch ( eFormat ) {

	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RGBA8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_BGRA8_UNORM:
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RGBA8_SRGB:
		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RGBA16_FLOAT:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_R8_UNORM:
		return DXGI_FORMAT_R8_UNORM;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RG8_UNORM:
		return DXGI_FORMAT_R8G8_UNORM;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_R16_FLOAT:
		return DXGI_FORMAT_R16_FLOAT;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_R32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_R32_UINT:
		return DXGI_FORMAT_R32_UINT;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RG32_FLOAT:
		return DXGI_FORMAT_R32G32_FLOAT;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RGBA32_FLOAT:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_D16_UNORM:
		return DXGI_FORMAT_D16_UNORM;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_D32_FLOAT:
		return DXGI_FORMAT_D32_FLOAT;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_BC1_UNORM:
		return DXGI_FORMAT_BC1_UNORM;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_BC3_UNORM:
		return DXGI_FORMAT_BC3_UNORM;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_BC5_UNORM:
		return DXGI_FORMAT_BC5_UNORM;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_BC7_UNORM:
		return DXGI_FORMAT_BC7_UNORM;
	default:
		return DXGI_FORMAT_UNKNOWN;
	}
}

static uint32_t sirD3D12VertexFormatBytes( SIRGraphicsFormat eFormat ) noexcept
{
	switch ( eFormat ) {
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_R8_UNORM:
		return 1;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RG8_UNORM:
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_R16_FLOAT:
		return 2;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RGBA8_UNORM:
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_BGRA8_UNORM:
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_R32_FLOAT:
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_R32_UINT:
		return 4;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RG32_FLOAT:
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RGBA16_FLOAT:
		return 8;
	case SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_RGBA32_FLOAT:
		return 16;
	default:
		return 0;
	}
}

static bool sirD3D12ValidDxil( const sirGraphicsShaderCode_t& shader ) noexcept
{
	return shader.eFormat == SIRGraphicsShaderFormat::SIR_GRAPHICS_SHADER_DXIL && sirD3D12ValidateDXILContainer( shader.pBytes, shader.nByteCount );
}

static void sirD3D12HashValue( uint64_t *pFirst, uint64_t *pSecond, uint64_t nValue ) noexcept
{
	for ( uint32_t iByte = 0; iByte < 8; ++iByte ) {
		const uint64_t nByte = ( nValue >> ( iByte * 8 ) ) & 0xffU;
		*pFirst = ( *pFirst ^ nByte ) * 1099511628211ULL;
		*pSecond = ( *pSecond ^ nByte ) * 14029467366897019727ULL;
	}
}

static void sirD3D12HashBytes( uint64_t *pFirst, uint64_t *pSecond, const void *pData, uint64_t nBytes ) noexcept
{
	const uint8_t *pBytes = static_cast<const uint8_t *>( pData );
	sirD3D12HashValue( pFirst, pSecond, nBytes );
	for ( uint64_t iByte = 0; iByte < nBytes; ++iByte ) {
		*pFirst = ( *pFirst ^ pBytes[iByte] ) * 1099511628211ULL;
		*pSecond = ( *pSecond ^ pBytes[iByte] ) * 14029467366897019727ULL;
	}
}

static void sirD3D12HashShader( uint64_t *pFirst, uint64_t *pSecond, const sirGraphicsShaderCode_t& shader ) noexcept
{
	sirD3D12HashValue( pFirst, pSecond, shader.eFormat );
	if ( shader.pBytes == nullptr || shader.nByteCount == 0 ) {
		sirD3D12HashValue( pFirst, pSecond, 0 );
		return;
	}
	sirD3D12HashBytes( pFirst, pSecond, shader.pBytes, shader.nByteCount );
}

static void sirD3D12PipelineCacheName( const sirGraphicsPipelineDesc_t& desc, wchar_t *pszName ) noexcept
{
	uint64_t nFirst = 14695981039346656037ULL;
	uint64_t nSecond = 7809847782465536322ULL;
	sirD3D12HashValue( &nFirst, &nSecond, 1 );
	sirD3D12HashValue( &nFirst, &nSecond, desc.eType );
	sirD3D12HashValue( &nFirst, &nSecond, desc.eModel );
	sirD3D12HashValue( &nFirst, &nSecond, desc.nBindingCount );
	for ( uint32_t i = 0; i < desc.nBindingCount; ++i ) {
		const sirGraphicsBindingDesc_t& binding = desc.pBindings[i];
		sirD3D12HashValue( &nFirst, &nSecond, binding.nBinding );
		sirD3D12HashValue( &nFirst, &nSecond, binding.nSpace );
		sirD3D12HashValue( &nFirst, &nSecond, binding.nArrayCount );
		sirD3D12HashValue( &nFirst, &nSecond, binding.nShaderStageMask );
		sirD3D12HashValue( &nFirst, &nSecond, binding.eType );
	}
	if ( desc.eType == SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_COMPUTE ) {
		sirD3D12HashShader( &nFirst, &nSecond, desc.computeShader );
	} else {
		sirD3D12HashShader( &nFirst, &nSecond, desc.vertexShader );
		sirD3D12HashShader( &nFirst, &nSecond, desc.pixelShader );
		sirD3D12HashValue( &nFirst, &nSecond, desc.nAttributeCount );
		for ( uint32_t i = 0; i < desc.nAttributeCount; ++i ) {
			const sirGraphicsVertexAttribute_t& attribute = desc.pAttributes[i];
			sirD3D12HashValue( &nFirst, &nSecond, attribute.nLocation );
			sirD3D12HashValue( &nFirst, &nSecond, attribute.nBufferSlot );
			sirD3D12HashValue( &nFirst, &nSecond, attribute.nOffsetBytes );
			sirD3D12HashValue( &nFirst, &nSecond, attribute.nStrideBytes );
			sirD3D12HashValue( &nFirst, &nSecond, attribute.eFormat );
		}
		sirD3D12HashValue( &nFirst, &nSecond, desc.nColorFormatCount );
		for ( uint32_t i = 0; i < desc.nColorFormatCount; ++i ) {
			sirD3D12HashValue( &nFirst, &nSecond, desc.szColorFormats[i] );
			const sirGraphicsBlendState_t& blend = desc.szBlendStates[i];
			sirD3D12HashValue( &nFirst, &nSecond, blend.eSourceColor );
			sirD3D12HashValue( &nFirst, &nSecond, blend.eDestinationColor );
			sirD3D12HashValue( &nFirst, &nSecond, blend.eColorOperation );
			sirD3D12HashValue( &nFirst, &nSecond, blend.eSourceAlpha );
			sirD3D12HashValue( &nFirst, &nSecond, blend.eDestinationAlpha );
			sirD3D12HashValue( &nFirst, &nSecond, blend.eAlphaOperation );
			sirD3D12HashValue( &nFirst, &nSecond, blend.nColorWriteMask );
			sirD3D12HashValue( &nFirst, &nSecond, blend.bEnabled );
		}
		sirD3D12HashValue( &nFirst, &nSecond, desc.eDepthFormat );
		sirD3D12HashValue( &nFirst, &nSecond, desc.nSampleCount );
		sirD3D12HashValue( &nFirst, &nSecond, desc.eTopology );
		sirD3D12HashValue( &nFirst, &nSecond, desc.eCullMode );
		sirD3D12HashValue( &nFirst, &nSecond, desc.eDepthCompare );
		sirD3D12HashValue( &nFirst, &nSecond, desc.bDepthTest );
		sirD3D12HashValue( &nFirst, &nSecond, desc.bDepthWrite );
	}
	const wchar_t *pszHex = L"0123456789abcdef";
	for ( uint32_t i = 0; i < 16; ++i ) {
		pszName[i] = pszHex[( nFirst >> ( ( 15 - i ) * 4 ) ) & 15U];
		pszName[16 + i] = pszHex[( nSecond >> ( ( 15 - i ) * 4 ) ) & 15U];
	}
	pszName[32] = 0;
}

static D3D12_DESCRIPTOR_RANGE_TYPE sirD3D12RangeType( SIRGraphicsBindingType eType ) noexcept
{
	switch ( eType ) {
	case SIRGraphicsBindingType::SIR_GRAPHICS_BINDING_CONSTANT_BUFFER:
		return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	case SIRGraphicsBindingType::SIR_GRAPHICS_BINDING_SAMPLED_TEXTURE:
		return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	case SIRGraphicsBindingType::SIR_GRAPHICS_BINDING_STORAGE_BUFFER:
	case SIRGraphicsBindingType::SIR_GRAPHICS_BINDING_STORAGE_TEXTURE:
		return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	case SIRGraphicsBindingType::SIR_GRAPHICS_BINDING_SAMPLER:
		return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
	default:
		return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	}
}

static D3D12_SHADER_VISIBILITY sirD3D12ShaderVisibility( uint32_t nStages ) noexcept
{
	if ( nStages == SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_VERTEX ) {
		return D3D12_SHADER_VISIBILITY_VERTEX;
	}
	if ( nStages == SIRGraphicsShaderStage::SIR_GRAPHICS_STAGE_FRAGMENT ) {
		return D3D12_SHADER_VISIBILITY_PIXEL;
	}
	return D3D12_SHADER_VISIBILITY_ALL;
}

static D3D12_CULL_MODE sirD3D12Cull( SIRGraphicsCullMode eCull ) noexcept
{
	if ( eCull == SIRGraphicsCullMode::SIR_GRAPHICS_CULL_FRONT ) {
		return D3D12_CULL_MODE_FRONT;
	}
	if ( eCull == SIRGraphicsCullMode::SIR_GRAPHICS_CULL_BACK ) {
		return D3D12_CULL_MODE_BACK;
	}
	return D3D12_CULL_MODE_NONE;
}

static D3D12_COMPARISON_FUNC sirD3D12Compare( SIRGraphicsCompareOp eCompare ) noexcept
{
	switch ( eCompare ) {
	case SIRGraphicsCompareOp::SIR_GRAPHICS_COMPARE_NEVER:
		return D3D12_COMPARISON_FUNC_NEVER;
	case SIRGraphicsCompareOp::SIR_GRAPHICS_COMPARE_LESS:
		return D3D12_COMPARISON_FUNC_LESS;
	case SIRGraphicsCompareOp::SIR_GRAPHICS_COMPARE_EQUAL:
		return D3D12_COMPARISON_FUNC_EQUAL;
	case SIRGraphicsCompareOp::SIR_GRAPHICS_COMPARE_LESS_EQUAL:
		return D3D12_COMPARISON_FUNC_LESS_EQUAL;
	case SIRGraphicsCompareOp::SIR_GRAPHICS_COMPARE_GREATER:
		return D3D12_COMPARISON_FUNC_GREATER;
	case SIRGraphicsCompareOp::SIR_GRAPHICS_COMPARE_NOT_EQUAL:
		return D3D12_COMPARISON_FUNC_NOT_EQUAL;
	case SIRGraphicsCompareOp::SIR_GRAPHICS_COMPARE_GREATER_EQUAL:
		return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
	case SIRGraphicsCompareOp::SIR_GRAPHICS_COMPARE_ALWAYS:
		return D3D12_COMPARISON_FUNC_ALWAYS;
	default:
		return D3D12_COMPARISON_FUNC_ALWAYS;
	}
}

static D3D12_BLEND sirD3D12BlendFactor( SIRGraphicsBlendFactor eFactor ) noexcept
{
	switch ( eFactor ) {
	case SIRGraphicsBlendFactor::SIR_GRAPHICS_BLEND_ZERO:
		return D3D12_BLEND_ZERO;
	case SIRGraphicsBlendFactor::SIR_GRAPHICS_BLEND_ONE:
		return D3D12_BLEND_ONE;
	case SIRGraphicsBlendFactor::SIR_GRAPHICS_BLEND_SRC_COLOR:
		return D3D12_BLEND_SRC_COLOR;
	case SIRGraphicsBlendFactor::SIR_GRAPHICS_BLEND_ONE_MINUS_SRC_COLOR:
		return D3D12_BLEND_INV_SRC_COLOR;
	case SIRGraphicsBlendFactor::SIR_GRAPHICS_BLEND_DST_COLOR:
		return D3D12_BLEND_DEST_COLOR;
	case SIRGraphicsBlendFactor::SIR_GRAPHICS_BLEND_ONE_MINUS_DST_COLOR:
		return D3D12_BLEND_INV_DEST_COLOR;
	case SIRGraphicsBlendFactor::SIR_GRAPHICS_BLEND_SRC_ALPHA:
		return D3D12_BLEND_SRC_ALPHA;
	case SIRGraphicsBlendFactor::SIR_GRAPHICS_BLEND_ONE_MINUS_SRC_ALPHA:
		return D3D12_BLEND_INV_SRC_ALPHA;
	case SIRGraphicsBlendFactor::SIR_GRAPHICS_BLEND_DST_ALPHA:
		return D3D12_BLEND_DEST_ALPHA;
	case SIRGraphicsBlendFactor::SIR_GRAPHICS_BLEND_ONE_MINUS_DST_ALPHA:
		return D3D12_BLEND_INV_DEST_ALPHA;
	default:
		return D3D12_BLEND_ONE;
	}
}

static D3D12_BLEND_OP sirD3D12BlendOperation( SIRGraphicsBlendOp eOperation ) noexcept
{
	switch ( eOperation ) {
	case SIRGraphicsBlendOp::SIR_GRAPHICS_BLEND_SUBTRACT:
		return D3D12_BLEND_OP_SUBTRACT;
	case SIRGraphicsBlendOp::SIR_GRAPHICS_BLEND_REVERSE_SUBTRACT:
		return D3D12_BLEND_OP_REV_SUBTRACT;
	case SIRGraphicsBlendOp::SIR_GRAPHICS_BLEND_MIN:
		return D3D12_BLEND_OP_MIN;
	case SIRGraphicsBlendOp::SIR_GRAPHICS_BLEND_MAX:
		return D3D12_BLEND_OP_MAX;
	default:
		return D3D12_BLEND_OP_ADD;
	}
}

static bool sirD3D12ValidateAttributes( const sirGraphicsPipelineDesc_t& desc ) noexcept
{
	for ( uint32_t iAttribute = 0; iAttribute < desc.nAttributeCount; ++iAttribute ) {
		const sirGraphicsVertexAttribute_t& attribute = desc.pAttributes[iAttribute];
		const uint32_t nBytes = sirD3D12VertexFormatBytes( attribute.eFormat );
		if ( nBytes == 0 || attribute.nLocation >= 32 || attribute.nBufferSlot >= 16 || attribute.nStrideBytes == 0 ) {
			return false;
		}
		if ( attribute.nOffsetBytes > attribute.nStrideBytes || nBytes > attribute.nStrideBytes - attribute.nOffsetBytes ) {
			return false;
		}
		for ( uint32_t iPrior = 0; iPrior < iAttribute; ++iPrior ) {
			if ( desc.pAttributes[iPrior].nLocation == attribute.nLocation ) {
				return false;
			}
		}
	}
	return true;
}

SIRD3D12Pipelines::~SIRD3D12Pipelines( void ) noexcept
{
	shutdown( false );
}

bool SIRD3D12Pipelines::initialize( SIRD3D12Device *pNative, uint32_t nGeneration ) noexcept
{
	if ( m_pNative != nullptr || pNative == nullptr || !pNative->isReady() || nGeneration == 0 ) {
		return false;
	}
	m_pDevice = pNative->getNativeDevice();
	if ( m_pDevice == nullptr ) {
		return false;
	}
	m_pNative = pNative;
	m_nDeviceGeneration = nGeneration;
	D3D12_FEATURE_DATA_SHADER_CACHE cacheCaps {};
	const bool bLibrarySupported =
	    SUCCEEDED( m_pDevice->CheckFeatureSupport( D3D12_FEATURE_SHADER_CACHE, &cacheCaps, sizeof( cacheCaps ) ) ) &&
	    ( cacheCaps.SupportFlags & D3D12_SHADER_CACHE_SUPPORT_LIBRARY ) != 0;
	Microsoft::WRL::ComPtr<ID3D12Device1> pDevice1;
	if ( bLibrarySupported && SUCCEEDED( m_pDevice->QueryInterface( IID_PPV_ARGS( pDevice1.GetAddressOf() ) ) ) ) {
		static_cast<void>(pDevice1->CreatePipelineLibrary( nullptr, 0, IID_PPV_ARGS( m_pPipelineLibrary.GetAddressOf() ) ));
	}
	m_CacheStats.bAvailable = m_pPipelineLibrary != nullptr;
	return true;
}

SIRGraphicsResult SIRD3D12Pipelines::loadPipelineCache( const void *pData, uint64_t nBytes ) noexcept
{
	if ( m_pNative == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( pData == nullptr || nBytes == 0 || nBytes > 16ULL * 1024ULL * 1024ULL || nBytes > SIZE_MAX ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( m_bPipelineCreated ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	Microsoft::WRL::ComPtr<ID3D12Device1> pDevice1;
	if ( FAILED( m_pDevice->QueryInterface( IID_PPV_ARGS( pDevice1.GetAddressOf() ) ) ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	// The runtime may retain the imported database memory. The driver owns
	// a bounded copy until the library is released, independently of the caller.
	void *pOwnedBlob = VirtualAlloc( nullptr, static_cast<SIZE_T>(nBytes), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE );
	if ( pOwnedBlob == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_OUT_OF_MEMORY;
	}
	memcpy( pOwnedBlob, pData, static_cast<SIZE_T>(nBytes) );
	Microsoft::WRL::ComPtr<ID3D12PipelineLibrary> pImported;
	const HRESULT nResult = pDevice1->CreatePipelineLibrary( pOwnedBlob, static_cast<SIZE_T>(nBytes), IID_PPV_ARGS( pImported.GetAddressOf() ) );
	if ( FAILED( nResult ) ) {
		VirtualFree( pOwnedBlob, 0, MEM_RELEASE );
		return nResult == E_OUTOFMEMORY ? SIRGraphicsResult::SIR_GRAPHICS_OUT_OF_MEMORY : SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	m_pPipelineLibrary = pImported;
	if ( m_pLibraryBlob != nullptr ) {
		VirtualFree( m_pLibraryBlob, 0, MEM_RELEASE );
	}
	m_pLibraryBlob = pOwnedBlob;
	m_CacheStats = {};
	m_CacheStats.bAvailable = true;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRD3D12Pipelines::serializePipelineCache( void *pData, uint64_t *pBytes ) const noexcept
{
	if ( pBytes == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( m_pNative == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( m_pPipelineLibrary == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	const SIZE_T nRequired = m_pPipelineLibrary->GetSerializedSize();
	const uint64_t nCapacity = *pBytes;
	*pBytes = nRequired;
	if ( pData == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
	}
	if ( nCapacity < nRequired ) {
		return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	const HRESULT nResult = m_pPipelineLibrary->Serialize( pData, nRequired );
	return SUCCEEDED( nResult ) ? SIRGraphicsResult::SIR_GRAPHICS_SUCCESS : SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
}

SIRGraphicsResult SIRD3D12Pipelines::queryPipelineCacheStats( sirD3D12PipelineCacheStats_t *pStats ) const noexcept
{
	if ( pStats == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pStats = {};
	if ( m_pNative == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DEVICE_LOST;
	}
	*pStats = m_CacheStats;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

void SIRD3D12Pipelines::releaseSlot( uint32_t iSlot ) noexcept
{
	sirD3D12PipelineSlot_t& slot = m_szSlots[iSlot];
	if ( !slot.bLive && !slot.bRetiring ) {
		return;
	}
	const uint32_t nGeneration = slot.nGeneration;
	slot.pPipelineState.Reset();
	slot.pRootSignature.Reset();
	slot = {};
	slot.nGeneration = nGeneration;
}

void SIRD3D12Pipelines::shutdown( bool bGpuComplete ) noexcept
{
	for ( uint32_t iSlot = 0; iSlot < s_nPipelineCapacity; ++iSlot ) {
		if ( bGpuComplete ) {
			releaseSlot( iSlot );
		} else {
			// Keep native PSO and root signature alive when GPU completion is unknown.
			static_cast<void>(m_szSlots)[iSlot].pPipelineState.Detach();
			static_cast<void>(m_szSlots)[iSlot].pRootSignature.Detach();
			const uint32_t nGeneration = m_szSlots[iSlot].nGeneration;
			m_szSlots[iSlot] = {};
			m_szSlots[iSlot].nGeneration = nGeneration;
		}
	}
	if ( bGpuComplete ) {
		m_pPipelineLibrary.Reset();
		if ( m_pLibraryBlob != nullptr ) {
			VirtualFree( m_pLibraryBlob, 0, MEM_RELEASE );
		}
	} else {
		// Preserve storage if a GPU-dependent library is intentionally detached.
		static_cast<void>(m_pPipelineLibrary.Detach());
	}
	m_pLibraryBlob = nullptr;
	m_CacheStats = {};
	m_bPipelineCreated = false;
	m_pNative = nullptr;
	m_pDevice = nullptr;
	m_nDeviceGeneration = 0;
}

sirD3D12PipelineSlot_t *SIRD3D12Pipelines::findSlot( sirGraphicsHandle_t hPipeline ) noexcept
{
	if ( m_pNative == nullptr || hPipeline.nSlot >= s_nPipelineCapacity ||
	     !sirGraphicsIsHandleValid( hPipeline, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_PIPELINE, m_nDeviceGeneration ) ) {
		return nullptr;
	}
	sirD3D12PipelineSlot_t& slot = m_szSlots[hPipeline.nSlot];
	if ( !slot.bLive || slot.nGeneration != hPipeline.nGeneration ) {
		return nullptr;
	}
	return &slot;
}

const sirD3D12PipelineSlot_t *SIRD3D12Pipelines::querySlot( sirGraphicsHandle_t hPipeline ) noexcept
{
	return findSlot( hPipeline );
}

SIRGraphicsResult SIRD3D12Pipelines::retainCommandReference( sirGraphicsHandle_t hPipeline ) noexcept
{
	sirD3D12PipelineSlot_t *pSlot = findSlot( hPipeline );
	if ( pSlot == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pSlot->nCommandReferences == UINT32_MAX ) {
		return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	++pSlot->nCommandReferences;
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

void SIRD3D12Pipelines::releaseCommandReference( sirGraphicsHandle_t hPipeline ) noexcept
{
	sirD3D12PipelineSlot_t *pSlot = findSlot( hPipeline );
	if ( pSlot != nullptr && pSlot->nCommandReferences != 0 ) {
		--pSlot->nCommandReferences;
	}
}

void SIRD3D12Pipelines::collectRetired( void ) noexcept
{
	if ( m_pNative == nullptr ) {
		return;
	}
	for ( uint32_t iSlot = 0; iSlot < s_nPipelineCapacity; ++iSlot ) {
		sirD3D12PipelineSlot_t& slot = m_szSlots[iSlot];
		if ( !slot.bRetiring ) {
			continue;
		}
		bool bComplete = false;
		if ( !m_pNative->pollCompletionSet( slot.retireCompletion, &bComplete ) ) {
			return;
		}
		if ( bComplete ) {
			releaseSlot( iSlot );
		}
	}
}

HRESULT SIRD3D12Pipelines::createRootSignature( const sirGraphicsPipelineDesc_t& desc, ID3D12RootSignature **ppRoot ) noexcept
{
	D3D12_DESCRIPTOR_RANGE szRanges[32];
	memset( szRanges, 0, sizeof( szRanges ) );
	D3D12_ROOT_PARAMETER szParameters[32];
	memset( szParameters, 0, sizeof( szParameters ) );
	for ( uint32_t iBinding = 0; iBinding < desc.nBindingCount; ++iBinding ) {
		const sirGraphicsBindingDesc_t& binding = desc.pBindings[iBinding];
		szRanges[iBinding].RangeType = sirD3D12RangeType( binding.eType );
		szRanges[iBinding].NumDescriptors = binding.nArrayCount;
		szRanges[iBinding].BaseShaderRegister = binding.nBinding;
		szRanges[iBinding].RegisterSpace = binding.nSpace;
		szRanges[iBinding].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		szParameters[iBinding].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		szParameters[iBinding].DescriptorTable.NumDescriptorRanges = 1;
		szParameters[iBinding].DescriptorTable.pDescriptorRanges = &szRanges[iBinding];
		szParameters[iBinding].ShaderVisibility = sirD3D12ShaderVisibility( binding.nShaderStageMask );
	}
	D3D12_ROOT_SIGNATURE_DESC root {};
	root.NumParameters = desc.nBindingCount;
	root.pParameters = desc.nBindingCount == 0 ? nullptr : szParameters;
	root.Flags = desc.eType == SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_GRAPHICS ? D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
	                                                          : D3D12_ROOT_SIGNATURE_FLAG_NONE;
	Microsoft::WRL::ComPtr<ID3DBlob> pRootBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> pErrors;
	const HRESULT nSerialized =
	    D3D12SerializeRootSignature( &root, D3D_ROOT_SIGNATURE_VERSION_1, pRootBlob.GetAddressOf(), pErrors.GetAddressOf() );
	if ( FAILED( nSerialized ) ) {
		return nSerialized;
	}
	return m_pDevice->CreateRootSignature( 0, pRootBlob->GetBufferPointer(), pRootBlob->GetBufferSize(), IID_PPV_ARGS( ppRoot ) );
}

HRESULT SIRD3D12Pipelines::createComputeState( const sirGraphicsPipelineDesc_t& desc, ID3D12RootSignature *pRoot,
                                               ID3D12PipelineState **ppPipeline, const wchar_t *pszCacheKey ) noexcept
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC native {};
	native.pRootSignature = pRoot;
	native.CS.pShaderBytecode = desc.computeShader.pBytes;
	native.CS.BytecodeLength = static_cast<SIZE_T>(desc.computeShader.nByteCount);
	if ( m_pPipelineLibrary != nullptr ) {
		if ( SUCCEEDED( m_pPipelineLibrary->LoadComputePipeline( pszCacheKey, &native, IID_PPV_ARGS( ppPipeline ) ) ) ) {
			++m_CacheStats.nCacheHits;
			return S_OK;
		}
		++m_CacheStats.nCacheMisses;
	}
	const HRESULT nResult = m_pDevice->CreateComputePipelineState( &native, IID_PPV_ARGS( ppPipeline ) );
	if ( SUCCEEDED( nResult ) && m_pPipelineLibrary != nullptr && m_CacheStats.nCacheStores < 256 &&
	     m_pPipelineLibrary->GetSerializedSize() < 16ULL * 1024ULL * 1024ULL &&
	     SUCCEEDED( m_pPipelineLibrary->StorePipeline( pszCacheKey, *ppPipeline ) ) ) {
		++m_CacheStats.nCacheStores;
	}
	return nResult;
}

static bool sirD3D12SupportsPipelineTarget( ID3D12Device *pDevice, DXGI_FORMAT eFormat, uint32_t nSampleCount,
                                            D3D12_FORMAT_SUPPORT1 eRequired ) noexcept
{
	if ( !sirD3D12SupportsSampleCount( pDevice, eFormat, nSampleCount ) ) {
		return false;
	}
	D3D12_FEATURE_DATA_FORMAT_SUPPORT support {};
	support.Format = eFormat;
	return SUCCEEDED( pDevice->CheckFeatureSupport( D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof( support ) ) ) &&
	       ( support.Support1 & eRequired ) == eRequired;
}

HRESULT SIRD3D12Pipelines::createGraphicsState( const sirGraphicsPipelineDesc_t& desc, ID3D12RootSignature *pRoot,
                                                ID3D12PipelineState **ppPipeline, const wchar_t *pszCacheKey ) noexcept
{
	D3D12_INPUT_ELEMENT_DESC szAttributes[32];
	memset( szAttributes, 0, sizeof( szAttributes ) );
	for ( uint32_t iAttribute = 0; iAttribute < desc.nAttributeCount; ++iAttribute ) {
		const sirGraphicsVertexAttribute_t& attribute = desc.pAttributes[iAttribute];
		D3D12_INPUT_ELEMENT_DESC& native = szAttributes[iAttribute];
		// Cooked DXIL uses the deterministic SIR vertex-location mapping:
		// location 0 = POSITION0, 1 = COLOR0, 2+ = TEXCOORD(location-2).
		native.SemanticName = attribute.nLocation == 0 ? "POSITION" : ( attribute.nLocation == 1 ? "COLOR" : "TEXCOORD" );
		native.SemanticIndex = attribute.nLocation < 2 ? 0 : attribute.nLocation - 2;
		native.Format = sirD3D12PipelineFormat( attribute.eFormat );
		native.InputSlot = attribute.nBufferSlot;
		native.AlignedByteOffset = attribute.nOffsetBytes;
		native.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC native {};
	native.pRootSignature = pRoot;
	native.VS = { desc.vertexShader.pBytes, static_cast<SIZE_T>(desc.vertexShader.nByteCount) };
	native.PS = { desc.pixelShader.pBytes, static_cast<SIZE_T>(desc.pixelShader.nByteCount) };
	native.SampleMask = UINT_MAX;
	native.InputLayout = { desc.nAttributeCount == 0 ? nullptr : szAttributes, desc.nAttributeCount };
	native.PrimitiveTopologyType = desc.eTopology == SIRGraphicsTopology::SIR_GRAPHICS_TOPOLOGY_POINTS ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT
	                               : ( desc.eTopology == SIRGraphicsTopology::SIR_GRAPHICS_TOPOLOGY_LINES || desc.eTopology == SIRGraphicsTopology::SIR_GRAPHICS_TOPOLOGY_LINE_STRIP )
	                                   ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE
	                                   : D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	native.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	native.RasterizerState.CullMode = sirD3D12Cull( desc.eCullMode );
	native.RasterizerState.FrontCounterClockwise = FALSE;
	native.RasterizerState.DepthClipEnable = TRUE;
	native.RasterizerState.MultisampleEnable = desc.nSampleCount > 1 ? TRUE : FALSE;
	native.DepthStencilState.DepthEnable = desc.bDepthTest ? TRUE : FALSE;
	native.DepthStencilState.DepthWriteMask = desc.bDepthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	native.DepthStencilState.DepthFunc = sirD3D12Compare( desc.eDepthCompare );
	native.DepthStencilState.StencilEnable = FALSE;
	native.DSVFormat = sirD3D12PipelineFormat( desc.eDepthFormat );
	native.NumRenderTargets = desc.nColorFormatCount;
	native.SampleDesc.Count = desc.nSampleCount;
	native.BlendState.IndependentBlendEnable = TRUE;
	for ( uint32_t iColor = 0; iColor < desc.nColorFormatCount; ++iColor ) {
		const sirGraphicsBlendState_t& blend = desc.szBlendStates[iColor];
		D3D12_RENDER_TARGET_BLEND_DESC& nativeBlend = native.BlendState.RenderTarget[iColor];
		native.RTVFormats[iColor] = sirD3D12PipelineFormat( desc.szColorFormats[iColor] );
		nativeBlend.BlendEnable = blend.bEnabled ? TRUE : FALSE;
		nativeBlend.LogicOpEnable = FALSE;
		nativeBlend.SrcBlend = sirD3D12BlendFactor( blend.eSourceColor );
		nativeBlend.DestBlend = sirD3D12BlendFactor( blend.eDestinationColor );
		nativeBlend.BlendOp = sirD3D12BlendOperation( blend.eColorOperation );
		nativeBlend.SrcBlendAlpha = sirD3D12BlendFactor( blend.eSourceAlpha );
		nativeBlend.DestBlendAlpha = sirD3D12BlendFactor( blend.eDestinationAlpha );
		nativeBlend.BlendOpAlpha = sirD3D12BlendOperation( blend.eAlphaOperation );
		nativeBlend.RenderTargetWriteMask = static_cast<UINT8>(blend.nColorWriteMask);
	}
	if ( m_pPipelineLibrary != nullptr ) {
		if ( SUCCEEDED( m_pPipelineLibrary->LoadGraphicsPipeline( pszCacheKey, &native, IID_PPV_ARGS( ppPipeline ) ) ) ) {
			++m_CacheStats.nCacheHits;
			return S_OK;
		}
		++m_CacheStats.nCacheMisses;
	}
	const HRESULT nResult = m_pDevice->CreateGraphicsPipelineState( &native, IID_PPV_ARGS( ppPipeline ) );
	if ( SUCCEEDED( nResult ) && m_pPipelineLibrary != nullptr && m_CacheStats.nCacheStores < 256 &&
	     m_pPipelineLibrary->GetSerializedSize() < 16ULL * 1024ULL * 1024ULL &&
	     SUCCEEDED( m_pPipelineLibrary->StorePipeline( pszCacheKey, *ppPipeline ) ) ) {
		++m_CacheStats.nCacheStores;
	}
	return nResult;
}

SIRGraphicsResult SIRD3D12Pipelines::createPipeline( const sirGraphicsPipelineDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pHandle = {};
	const SIRGraphicsResult eValid = sirGraphicsValidatePipeline( desc );
	if ( eValid != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eValid;
	}
	if ( m_pNative == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( desc.eType == SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	const bool bCompute = desc.eType == SIRGraphicsPipelineType::SIR_GRAPHICS_PIPELINE_COMPUTE;
	if ( !sirD3D12ValidDxil( bCompute ? desc.computeShader : desc.vertexShader ) ||
	     ( !bCompute && !sirD3D12ValidDxil( desc.pixelShader ) ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const SIRGraphicsResult eBindings = sirD3D12ValidateBindingLayout( desc.pBindings, desc.nBindingCount, desc.eType );
	if ( eBindings != SIRGraphicsResult::SIR_GRAPHICS_SUCCESS ) {
		return eBindings;
	}
	if ( !bCompute && !sirD3D12ValidateAttributes( desc ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !bCompute && desc.eTopology == SIRGraphicsTopology::SIR_GRAPHICS_TOPOLOGY_PATCHES ) {
		return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( !bCompute && desc.bDepthTest && desc.eDepthFormat == SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_UNKNOWN ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !bCompute && desc.bDepthWrite && !desc.bDepthTest ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !bCompute && desc.nColorFormatCount == 0 && !desc.bDepthTest ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !bCompute ) {
		for ( uint32_t iColor = 0; iColor < desc.nColorFormatCount; ++iColor ) {
			const DXGI_FORMAT eColorFormat = sirD3D12PipelineFormat( desc.szColorFormats[iColor] );
			if ( !sirD3D12SupportsPipelineTarget( m_pDevice, eColorFormat, desc.nSampleCount, D3D12_FORMAT_SUPPORT1_RENDER_TARGET ) ) {
				return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
			}
		}
		const DXGI_FORMAT eDepthFormat = sirD3D12PipelineFormat( desc.eDepthFormat );
		if ( desc.eDepthFormat != SIRGraphicsFormat::SIR_GRAPHICS_FORMAT_UNKNOWN &&
		     !sirD3D12SupportsPipelineTarget( m_pDevice, eDepthFormat, desc.nSampleCount, D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL ) ) {
			return SIRGraphicsResult::SIR_GRAPHICS_UNSUPPORTED;
		}
	}
	collectRetired();
	uint32_t iFree = s_nPipelineCapacity;
	for ( uint32_t iSlot = 0; iSlot < s_nPipelineCapacity; ++iSlot ) {
		if ( !m_szSlots[iSlot].bLive && !m_szSlots[iSlot].bRetiring && m_szSlots[iSlot].nGeneration < UINT32_MAX ) {
			iFree = iSlot;
			break;
		}
	}
	if ( iFree == s_nPipelineCapacity ) {
		return SIRGraphicsResult::SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}

	wchar_t szCacheKey[33];
	memset( szCacheKey, 0, sizeof( szCacheKey ) );
	sirD3D12PipelineCacheName( desc, szCacheKey );
	Microsoft::WRL::ComPtr<ID3D12RootSignature> pRoot;
	// Identical live binding layouts use one root signature even when the
	// shaders or fixed-function pipeline state differ.
	for ( uint32_t iSlot = 0; iSlot < s_nPipelineCapacity && pRoot == nullptr; ++iSlot ) {
		const sirD3D12PipelineSlot_t& prior = m_szSlots[iSlot];
		if ( !prior.bLive || prior.eType != desc.eType || prior.nBindingCount != desc.nBindingCount ) {
			continue;
		}
		bool bSame = true;
		for ( uint32_t iBinding = 0; iBinding < desc.nBindingCount; ++iBinding ) {
			const sirGraphicsBindingDesc_t& a = prior.szBindings[iBinding];
			const sirGraphicsBindingDesc_t& b = desc.pBindings[iBinding];
			if ( a.nBinding != b.nBinding || a.nSpace != b.nSpace || a.nArrayCount != b.nArrayCount ||
			     a.nShaderStageMask != b.nShaderStageMask || a.eType != b.eType ) {
				bSame = false;
				break;
			}
		}
		if ( bSame ) {
			pRoot = prior.pRootSignature;
			++m_CacheStats.nRootReuses;
		}
	}
	if ( pRoot == nullptr ) {
		const HRESULT nRoot = createRootSignature( desc, pRoot.GetAddressOf() );
		if ( FAILED( nRoot ) ) {
			return nRoot == E_OUTOFMEMORY ? SIRGraphicsResult::SIR_GRAPHICS_OUT_OF_MEMORY : SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
		}
	}
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pPipeline;
	const HRESULT nPipeline = bCompute ? createComputeState( desc, pRoot.Get(), pPipeline.GetAddressOf(), szCacheKey )
	                                   : createGraphicsState( desc, pRoot.Get(), pPipeline.GetAddressOf(), szCacheKey );
	if ( FAILED( nPipeline ) ) {
		return nPipeline == E_OUTOFMEMORY ? SIRGraphicsResult::SIR_GRAPHICS_OUT_OF_MEMORY : SIRGraphicsResult::SIR_GRAPHICS_DRIVER_ERROR;
	}

	sirD3D12PipelineSlot_t& slot = m_szSlots[iFree];
	const uint32_t nGeneration = slot.nGeneration + 1;
	slot = {};
	slot.nGeneration = nGeneration;
	slot.eType = desc.eType;
	slot.eTopology = desc.eTopology;
	slot.nBindingCount = desc.nBindingCount;
	for ( uint32_t iBinding = 0; iBinding < desc.nBindingCount; ++iBinding ) {
		slot.szBindings[iBinding] = desc.pBindings[iBinding];
	}
	slot.pRootSignature = pRoot;
	slot.pPipelineState = pPipeline;
	m_bPipelineCreated = true;
	slot.bLive = true;
	*pHandle = { iFree, nGeneration, m_nDeviceGeneration, SIRGraphicsHandleType::SIR_GRAPHICS_HANDLE_PIPELINE };
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

SIRGraphicsResult SIRD3D12Pipelines::retirePipeline( sirGraphicsHandle_t hPipeline,
                                                       const sirGraphicsCompletionSet_t& completion ) noexcept
{
	sirD3D12PipelineSlot_t *pSlot = findSlot( hPipeline );
	if ( pSlot == nullptr ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( !sirGraphicsIsCompletionSetValid( completion, m_nDeviceGeneration ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirD3D12CompletionSet_t native {};
	native.nDeviceGeneration = completion.nDeviceGeneration;
	native.nGraphicsFence = completion.szQueueValues[SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_DIRECT];
	native.nComputeFence = completion.szQueueValues[SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COMPUTE];
	native.nCopyFence = completion.szQueueValues[SIRGraphicsQueue::SIR_GRAPHICS_QUEUE_COPY];
	if ( pSlot->nCommandReferences != 0 ) {
		return SIRGraphicsResult::SIR_GRAPHICS_BUSY;
	}
	bool bCompleted = false;
	if ( !m_pNative->pollCompletionSet( native, &bCompleted ) ) {
		return SIRGraphicsResult::SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	pSlot->bLive = false;
	pSlot->bRetiring = true;
	pSlot->retireCompletion = native;
	if ( bCompleted ) {
		releaseSlot( hPipeline.nSlot );
	}
	return SIRGraphicsResult::SIR_GRAPHICS_SUCCESS;
}

bool SIRD3D12Pipelines::queryNative( sirGraphicsHandle_t hPipeline, ID3D12PipelineState **ppPipeline,
                                     ID3D12RootSignature **ppRoot ) noexcept
{
	if ( ppPipeline == nullptr || ppRoot == nullptr ) {
		return false;
	}
	*ppPipeline = nullptr;
	*ppRoot = nullptr;
	sirD3D12PipelineSlot_t *pSlot = findSlot( hPipeline );
	if ( pSlot == nullptr ) {
		return false;
	}
	*ppPipeline = pSlot->pPipelineState.Get();
	*ppRoot = pSlot->pRootSignature.Get();
	return true;
}
