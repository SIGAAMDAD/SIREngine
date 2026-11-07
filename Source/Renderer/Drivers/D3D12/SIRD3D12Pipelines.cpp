/*
===========================================================================
SIREngine MPLv2 Source Code
Copyright (C) 2023-2027 Noah Van Til

This Source Code Form is subject to the terms of the Mozilla Public
License, v2. If a copy of the MPL was not distributed with this
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

static DXGI_FORMAT sirD3D12PipelineFormat( sirGraphicsFormat_t eFormat ) noexcept
{
	switch ( eFormat ) {

	case SIR_GRAPHICS_FORMAT_RGBA8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case SIR_GRAPHICS_FORMAT_BGRA8_UNORM:
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	case SIR_GRAPHICS_FORMAT_RGBA8_SRGB:
		return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	case SIR_GRAPHICS_FORMAT_RGBA16_FLOAT:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case SIR_GRAPHICS_FORMAT_R8_UNORM:
		return DXGI_FORMAT_R8_UNORM;
	case SIR_GRAPHICS_FORMAT_RG8_UNORM:
		return DXGI_FORMAT_R8G8_UNORM;
	case SIR_GRAPHICS_FORMAT_R16_FLOAT:
		return DXGI_FORMAT_R16_FLOAT;
	case SIR_GRAPHICS_FORMAT_R32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case SIR_GRAPHICS_FORMAT_R32_UINT:
		return DXGI_FORMAT_R32_UINT;
	case SIR_GRAPHICS_FORMAT_RG32_FLOAT:
		return DXGI_FORMAT_R32G32_FLOAT;
	case SIR_GRAPHICS_FORMAT_RGBA32_FLOAT:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case SIR_GRAPHICS_FORMAT_D16_UNORM:
		return DXGI_FORMAT_D16_UNORM;
	case SIR_GRAPHICS_FORMAT_D24_UNORM_S8_UINT:
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case SIR_GRAPHICS_FORMAT_D32_FLOAT:
		return DXGI_FORMAT_D32_FLOAT;
	case SIR_GRAPHICS_FORMAT_BC1_UNORM:
		return DXGI_FORMAT_BC1_UNORM;
	case SIR_GRAPHICS_FORMAT_BC3_UNORM:
		return DXGI_FORMAT_BC3_UNORM;
	case SIR_GRAPHICS_FORMAT_BC5_UNORM:
		return DXGI_FORMAT_BC5_UNORM;
	case SIR_GRAPHICS_FORMAT_BC7_UNORM:
		return DXGI_FORMAT_BC7_UNORM;
	default:
		return DXGI_FORMAT_UNKNOWN;
	}
}

static uint32_t sirD3D12VertexFormatBytes( sirGraphicsFormat_t eFormat ) noexcept
{
	switch ( eFormat ) {
	case SIR_GRAPHICS_FORMAT_R8_UNORM:
		return 1;
	case SIR_GRAPHICS_FORMAT_RG8_UNORM:
	case SIR_GRAPHICS_FORMAT_R16_FLOAT:
		return 2;
	case SIR_GRAPHICS_FORMAT_RGBA8_UNORM:
	case SIR_GRAPHICS_FORMAT_BGRA8_UNORM:
	case SIR_GRAPHICS_FORMAT_R32_FLOAT:
	case SIR_GRAPHICS_FORMAT_R32_UINT:
		return 4;
	case SIR_GRAPHICS_FORMAT_RG32_FLOAT:
	case SIR_GRAPHICS_FORMAT_RGBA16_FLOAT:
		return 8;
	case SIR_GRAPHICS_FORMAT_RGBA32_FLOAT:
		return 16;
	default:
		return 0;
	}
}

static bool sirD3D12ValidDxil( const sirGraphicsShaderCode_t& shader ) noexcept
{
	constexpr uint64_t MAX_DXIL_BYTES = 16ULL * 1024ULL * 1024ULL;
	return shader.eFormat == SIR_GRAPHICS_SHADER_DXIL && shader.pBytes != nullptr && shader.nByteCount >= 16 &&
	       shader.nByteCount <= MAX_DXIL_BYTES && memcmp( shader.pBytes, "DXBC", 4 ) == 0;
}

static D3D12_DESCRIPTOR_RANGE_TYPE sirD3D12RangeType( sirGraphicsBindingType_t eType ) noexcept
{
	switch ( eType ) {
	case SIR_GRAPHICS_BINDING_CONSTANT_BUFFER:
		return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	case SIR_GRAPHICS_BINDING_SAMPLED_TEXTURE:
		return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	case SIR_GRAPHICS_BINDING_STORAGE_BUFFER:
	case SIR_GRAPHICS_BINDING_STORAGE_TEXTURE:
		return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	case SIR_GRAPHICS_BINDING_SAMPLER:
		return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
	default:
		return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	}
}

static D3D12_SHADER_VISIBILITY sirD3D12ShaderVisibility( uint32_t nStages ) noexcept
{
	if ( nStages == SIR_GRAPHICS_STAGE_VERTEX ) {
		return D3D12_SHADER_VISIBILITY_VERTEX;
	}
	if ( nStages == SIR_GRAPHICS_STAGE_FRAGMENT ) {
		return D3D12_SHADER_VISIBILITY_PIXEL;
	}
	return D3D12_SHADER_VISIBILITY_ALL;
}

static D3D12_CULL_MODE sirD3D12Cull( sirGraphicsCullMode_t eCull ) noexcept
{
	if ( eCull == SIR_GRAPHICS_CULL_FRONT ) {
		return D3D12_CULL_MODE_FRONT;
	}
	if ( eCull == SIR_GRAPHICS_CULL_BACK ) {
		return D3D12_CULL_MODE_BACK;
	}
	return D3D12_CULL_MODE_NONE;
}

static D3D12_COMPARISON_FUNC sirD3D12Compare( sirGraphicsCompareOp_t eCompare ) noexcept
{
	switch ( eCompare ) {
	case SIR_GRAPHICS_COMPARE_NEVER:
		return D3D12_COMPARISON_FUNC_NEVER;
	case SIR_GRAPHICS_COMPARE_LESS:
		return D3D12_COMPARISON_FUNC_LESS;
	case SIR_GRAPHICS_COMPARE_EQUAL:
		return D3D12_COMPARISON_FUNC_EQUAL;
	case SIR_GRAPHICS_COMPARE_LESS_EQUAL:
		return D3D12_COMPARISON_FUNC_LESS_EQUAL;
	case SIR_GRAPHICS_COMPARE_GREATER:
		return D3D12_COMPARISON_FUNC_GREATER;
	case SIR_GRAPHICS_COMPARE_NOT_EQUAL:
		return D3D12_COMPARISON_FUNC_NOT_EQUAL;
	case SIR_GRAPHICS_COMPARE_GREATER_EQUAL:
		return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
	case SIR_GRAPHICS_COMPARE_ALWAYS:
		return D3D12_COMPARISON_FUNC_ALWAYS;
	default:
		return D3D12_COMPARISON_FUNC_ALWAYS;
	}
}

static D3D12_BLEND sirD3D12BlendFactor( sirGraphicsBlendFactor_t eFactor ) noexcept
{
	switch ( eFactor ) {
	case SIR_GRAPHICS_BLEND_ZERO:
		return D3D12_BLEND_ZERO;
	case SIR_GRAPHICS_BLEND_ONE:
		return D3D12_BLEND_ONE;
	case SIR_GRAPHICS_BLEND_SRC_COLOR:
		return D3D12_BLEND_SRC_COLOR;
	case SIR_GRAPHICS_BLEND_ONE_MINUS_SRC_COLOR:
		return D3D12_BLEND_INV_SRC_COLOR;
	case SIR_GRAPHICS_BLEND_DST_COLOR:
		return D3D12_BLEND_DEST_COLOR;
	case SIR_GRAPHICS_BLEND_ONE_MINUS_DST_COLOR:
		return D3D12_BLEND_INV_DEST_COLOR;
	case SIR_GRAPHICS_BLEND_SRC_ALPHA:
		return D3D12_BLEND_SRC_ALPHA;
	case SIR_GRAPHICS_BLEND_ONE_MINUS_SRC_ALPHA:
		return D3D12_BLEND_INV_SRC_ALPHA;
	case SIR_GRAPHICS_BLEND_DST_ALPHA:
		return D3D12_BLEND_DEST_ALPHA;
	case SIR_GRAPHICS_BLEND_ONE_MINUS_DST_ALPHA:
		return D3D12_BLEND_INV_DEST_ALPHA;
	default:
		return D3D12_BLEND_ONE;
	}
}

static D3D12_BLEND_OP sirD3D12BlendOperation( sirGraphicsBlendOp_t eOperation ) noexcept
{
	switch ( eOperation ) {
	case SIR_GRAPHICS_BLEND_SUBTRACT:
		return D3D12_BLEND_OP_SUBTRACT;
	case SIR_GRAPHICS_BLEND_REVERSE_SUBTRACT:
		return D3D12_BLEND_OP_REV_SUBTRACT;
	case SIR_GRAPHICS_BLEND_MIN:
		return D3D12_BLEND_OP_MIN;
	case SIR_GRAPHICS_BLEND_MAX:
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
	return true;
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
			( void )m_szSlots[iSlot].pPipelineState.Detach();
			( void )m_szSlots[iSlot].pRootSignature.Detach();
			const uint32_t nGeneration = m_szSlots[iSlot].nGeneration;
			m_szSlots[iSlot] = {};
			m_szSlots[iSlot].nGeneration = nGeneration;
		}
	}
	m_pNative = nullptr;
	m_pDevice = nullptr;
	m_nDeviceGeneration = 0;
}

sirD3D12PipelineSlot_t *SIRD3D12Pipelines::findSlot( sirGraphicsHandle_t hPipeline ) noexcept
{
	if ( m_pNative == nullptr || hPipeline.nSlot >= s_nPipelineCapacity ||
	     !sirGraphicsIsHandleValid( hPipeline, SIR_GRAPHICS_HANDLE_PIPELINE, m_nDeviceGeneration ) ) {
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

sirGraphicsResult_t SIRD3D12Pipelines::retainCommandReference( sirGraphicsHandle_t hPipeline ) noexcept
{
	sirD3D12PipelineSlot_t *pSlot = findSlot( hPipeline );
	if ( pSlot == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( pSlot->nCommandReferences == UINT32_MAX ) {
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}
	++pSlot->nCommandReferences;
	return SIR_GRAPHICS_SUCCESS;
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
	D3D12_DESCRIPTOR_RANGE szRanges[32] = {};
	D3D12_ROOT_PARAMETER szParameters[32] = {};
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
	D3D12_ROOT_SIGNATURE_DESC root = {};
	root.NumParameters = desc.nBindingCount;
	root.pParameters = desc.nBindingCount == 0 ? nullptr : szParameters;
	root.Flags = desc.eType == SIR_GRAPHICS_PIPELINE_GRAPHICS ? D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
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
                                               ID3D12PipelineState **ppPipeline ) noexcept
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC native = {};
	native.pRootSignature = pRoot;
	native.CS.pShaderBytecode = desc.computeShader.pBytes;
	native.CS.BytecodeLength = ( SIZE_T )desc.computeShader.nByteCount;
	return m_pDevice->CreateComputePipelineState( &native, IID_PPV_ARGS( ppPipeline ) );
}

static bool sirD3D12SupportsPipelineTarget( ID3D12Device *pDevice, DXGI_FORMAT eFormat, uint32_t nSampleCount,
                                            D3D12_FORMAT_SUPPORT1 eRequired ) noexcept
{
	if ( !sirD3D12SupportsSampleCount( pDevice, eFormat, nSampleCount ) ) {
		return false;
	}
	D3D12_FEATURE_DATA_FORMAT_SUPPORT support = {};
	support.Format = eFormat;
	return SUCCEEDED( pDevice->CheckFeatureSupport( D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof( support ) ) ) &&
	       ( support.Support1 & eRequired ) == eRequired;
}

HRESULT SIRD3D12Pipelines::createGraphicsState( const sirGraphicsPipelineDesc_t& desc, ID3D12RootSignature *pRoot,
                                                ID3D12PipelineState **ppPipeline ) noexcept
{
	D3D12_INPUT_ELEMENT_DESC szAttributes[32] = {};
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

	D3D12_GRAPHICS_PIPELINE_STATE_DESC native = {};
	native.pRootSignature = pRoot;
	native.VS = { desc.vertexShader.pBytes, ( SIZE_T )desc.vertexShader.nByteCount };
	native.PS = { desc.pixelShader.pBytes, ( SIZE_T )desc.pixelShader.nByteCount };
	native.SampleMask = UINT_MAX;
	native.InputLayout = { desc.nAttributeCount == 0 ? nullptr : szAttributes, desc.nAttributeCount };
	native.PrimitiveTopologyType = desc.eTopology == SIR_GRAPHICS_TOPOLOGY_POINTS ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT
	                               : ( desc.eTopology == SIR_GRAPHICS_TOPOLOGY_LINES || desc.eTopology == SIR_GRAPHICS_TOPOLOGY_LINE_STRIP )
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
		nativeBlend.RenderTargetWriteMask = ( UINT8 )blend.nColorWriteMask;
	}
	return m_pDevice->CreateGraphicsPipelineState( &native, IID_PPV_ARGS( ppPipeline ) );
}

sirGraphicsResult_t SIRD3D12Pipelines::createPipeline( const sirGraphicsPipelineDesc_t& desc, sirGraphicsHandle_t *pHandle ) noexcept
{
	if ( pHandle == nullptr ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	*pHandle = {};
	const sirGraphicsResult_t eValid = sirGraphicsValidatePipeline( desc );
	if ( eValid != SIR_GRAPHICS_SUCCESS ) {
		return eValid;
	}
	if ( m_pNative == nullptr ) {
		return SIR_GRAPHICS_DEVICE_LOST;
	}
	if ( desc.eType == SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	const bool bCompute = desc.eType == SIR_GRAPHICS_PIPELINE_COMPUTE;
	if ( !sirD3D12ValidDxil( bCompute ? desc.computeShader : desc.vertexShader ) ||
	     ( !bCompute && !sirD3D12ValidDxil( desc.pixelShader ) ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	const sirGraphicsResult_t eBindings = sirD3D12ValidateBindingLayout( desc.pBindings, desc.nBindingCount, desc.eType );
	if ( eBindings != SIR_GRAPHICS_SUCCESS ) {
		return eBindings;
	}
	if ( !bCompute && !sirD3D12ValidateAttributes( desc ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !bCompute && desc.eTopology == SIR_GRAPHICS_TOPOLOGY_PATCHES ) {
		return SIR_GRAPHICS_UNSUPPORTED;
	}
	if ( !bCompute && desc.bDepthTest && desc.eDepthFormat == SIR_GRAPHICS_FORMAT_UNKNOWN ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !bCompute && desc.bDepthWrite && !desc.bDepthTest ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !bCompute && desc.nColorFormatCount == 0 && !desc.bDepthTest ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	if ( !bCompute ) {
		for ( uint32_t iColor = 0; iColor < desc.nColorFormatCount; ++iColor ) {
			const DXGI_FORMAT eColorFormat = sirD3D12PipelineFormat( desc.szColorFormats[iColor] );
			if ( !sirD3D12SupportsPipelineTarget( m_pDevice, eColorFormat, desc.nSampleCount, D3D12_FORMAT_SUPPORT1_RENDER_TARGET ) ) {
				return SIR_GRAPHICS_UNSUPPORTED;
			}
		}
		const DXGI_FORMAT eDepthFormat = sirD3D12PipelineFormat( desc.eDepthFormat );
		if ( desc.eDepthFormat != SIR_GRAPHICS_FORMAT_UNKNOWN &&
		     !sirD3D12SupportsPipelineTarget( m_pDevice, eDepthFormat, desc.nSampleCount, D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL ) ) {
			return SIR_GRAPHICS_UNSUPPORTED;
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
		return SIR_GRAPHICS_CAPACITY_EXCEEDED;
	}

	Microsoft::WRL::ComPtr<ID3D12RootSignature> pRoot;
	const HRESULT nRoot = createRootSignature( desc, pRoot.GetAddressOf() );
	if ( FAILED( nRoot ) ) {
		return nRoot == E_OUTOFMEMORY ? SIR_GRAPHICS_OUT_OF_MEMORY : SIR_GRAPHICS_DRIVER_ERROR;
	}
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pPipeline;
	const HRESULT nPipeline = bCompute ? createComputeState( desc, pRoot.Get(), pPipeline.GetAddressOf() )
	                                   : createGraphicsState( desc, pRoot.Get(), pPipeline.GetAddressOf() );
	if ( FAILED( nPipeline ) ) {
		return nPipeline == E_OUTOFMEMORY ? SIR_GRAPHICS_OUT_OF_MEMORY : SIR_GRAPHICS_DRIVER_ERROR;
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
	slot.bLive = true;
	*pHandle = { iFree, nGeneration, m_nDeviceGeneration, SIR_GRAPHICS_HANDLE_PIPELINE };
	return SIR_GRAPHICS_SUCCESS;
}

sirGraphicsResult_t SIRD3D12Pipelines::retirePipeline( sirGraphicsHandle_t hPipeline,
                                                       const sirGraphicsCompletionSet_t& completion ) noexcept
{
	sirD3D12PipelineSlot_t *pSlot = findSlot( hPipeline );
	if ( pSlot == nullptr ) {
		return SIR_GRAPHICS_INVALID_HANDLE;
	}
	if ( !sirGraphicsIsCompletionSetValid( completion, m_nDeviceGeneration ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	sirD3D12CompletionSet_t native = {};
	native.nDeviceGeneration = completion.nDeviceGeneration;
	native.nGraphicsFence = completion.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT];
	native.nComputeFence = completion.szQueueValues[SIR_GRAPHICS_QUEUE_COMPUTE];
	native.nCopyFence = completion.szQueueValues[SIR_GRAPHICS_QUEUE_COPY];
	if ( pSlot->nCommandReferences != 0 ) {
		return SIR_GRAPHICS_BUSY;
	}
	bool bCompleted = false;
	if ( !m_pNative->pollCompletionSet( native, &bCompleted ) ) {
		return SIR_GRAPHICS_INVALID_ARGUMENT;
	}
	pSlot->bLive = false;
	pSlot->bRetiring = true;
	pSlot->retireCompletion = native;
	if ( bCompleted ) {
		releaseSlot( hPipeline.nSlot );
	}
	return SIR_GRAPHICS_SUCCESS;
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
