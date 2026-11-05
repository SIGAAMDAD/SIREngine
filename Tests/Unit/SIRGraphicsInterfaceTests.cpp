#include <Renderer/SIRGraphicsRegistry.h>
#include <Renderer/SIRGraphicsABIOperations.h>
#include <Renderer/SIRGraphicsValidation.h>

#include <stdio.h>

class SIRGraphicsTestDriver final : public SIRGraphicsDriver
{
private:
	sirGraphicsBackend_t m_eBackend;
	sirGraphicsAdapterInfo_t m_Adapter = {};
	uint32_t m_nAdapterCount = 1;
	sirGraphicsResult_t m_eCountResult = SIR_GRAPHICS_SUCCESS;
	sirGraphicsResult_t m_eQueryResult = SIR_GRAPHICS_SUCCESS;

public:
	SIRGraphicsTestDriver( sirGraphicsBackend_t eBackend, uint64_t nFeatures, uint32_t nMajor, uint32_t nMinor, bool bSoftware ) noexcept
	    : m_eBackend( eBackend )
	{
		m_Adapter.nAdapterIndex = 0;
		m_Adapter.nApiVersion = { nMajor, nMinor, 0 };
		m_Adapter.nFeatureMask = nFeatures;
		m_Adapter.bSoftwareAdapter = bSoftware;
	}

	void setAdapterCount( uint32_t nCount ) noexcept
	{
		m_nAdapterCount = nCount;
	}

	void setProviderResults( sirGraphicsResult_t eCount, sirGraphicsResult_t eQuery ) noexcept
	{
		m_eCountResult = eCount;
		m_eQueryResult = eQuery;
	}

	void setMemory( uint64_t nDedicated, uint64_t nShared ) noexcept
	{
		m_Adapter.nDedicatedVideoBytes = nDedicated;
		m_Adapter.nSharedSystemBytes = nShared;
	}

	sirGraphicsBackend_t getBackend( void ) const noexcept override
	{
		return m_eBackend;
	}

	sirGraphicsResult_t getAdapterCount( uint32_t *pCount ) const noexcept override
	{
		if ( pCount == nullptr ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( m_eCountResult != SIR_GRAPHICS_SUCCESS ) {
			return m_eCountResult;
		}
		*pCount = m_nAdapterCount;
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t queryAdapter( uint32_t nAdapterIndex, sirGraphicsAdapterInfo_t *pInfo ) const noexcept override
	{
		if ( pInfo == nullptr || nAdapterIndex >= m_nAdapterCount ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		if ( m_eQueryResult != SIR_GRAPHICS_SUCCESS ) {
			return m_eQueryResult;
		}
		*pInfo = m_Adapter;
		pInfo->nAdapterIndex = nAdapterIndex;
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t createDevice( const sirGraphicsDeviceCreateDesc_t &desc, SIRRenderingDevice **ppDevice ) noexcept override
	{
		( void )desc;
		if ( ppDevice == nullptr ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		*ppDevice = nullptr;
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	sirGraphicsResult_t destroyDevice( SIRRenderingDevice *pDevice ) noexcept override
	{
		( void )pDevice;
		return SIR_GRAPHICS_UNSUPPORTED;
	}
};

class SIRGraphicsTestCommandList final : public SIRGraphicsCommandList
{
private:
	sirGraphicsQueue_t m_eQueue;

public:
	explicit SIRGraphicsTestCommandList( sirGraphicsQueue_t eQueue ) noexcept : m_eQueue( eQueue )
	{
	}

	sirGraphicsQueue_t getQueue( void ) const noexcept override
	{
		return m_eQueue;
	}

	sirGraphicsResult_t reset( void ) noexcept override
	{
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t barrier( const sirGraphicsBarrier_t *pBarriers, uint32_t nCount ) noexcept override
	{
		( void )pBarriers;
		( void )nCount;
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t copyBuffer( sirGraphicsHandle_t hDestination, uint64_t nDstOffset, sirGraphicsHandle_t hSource, uint64_t nSrcOffset,
	                                uint64_t nBytes ) noexcept override
	{
		( void )hDestination;
		( void )nDstOffset;
		( void )hSource;
		( void )nSrcOffset;
		( void )nBytes;
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t copyBufferTexture( const sirGraphicsBufferTextureCopy_t &copy ) noexcept override
	{
		( void )copy;
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	sirGraphicsResult_t beginRenderPass( const sirGraphicsRenderPass_t &pass ) noexcept override
	{
		( void )pass;
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t endRenderPass( void ) noexcept override
	{
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t bindPipeline( sirGraphicsHandle_t hPipeline ) noexcept override
	{
		( void )hPipeline;
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t bindBindingSet( uint32_t nSetIndex, sirGraphicsHandle_t hBindingSet ) noexcept override
	{
		( void )nSetIndex;
		( void )hBindingSet;
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t bindVertexBuffer( uint32_t nSlot, sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes,
	                                      uint32_t nStrideBytes ) noexcept override
	{
		( void )nSlot;
		( void )hBuffer;
		( void )nOffsetBytes;
		( void )nStrideBytes;
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t bindIndexBuffer( sirGraphicsHandle_t hBuffer, uint64_t nOffsetBytes, bool bIndex32 ) noexcept override
	{
		( void )hBuffer;
		( void )nOffsetBytes;
		( void )bIndex32;
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t setViewport( const sirGraphicsViewport_t &viewport, const sirGraphicsRect_t &scissor ) noexcept override
	{
		( void )viewport;
		( void )scissor;
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t draw( uint32_t nVertices, uint32_t nInstances, uint32_t nFirstVertex, uint32_t nFirstInstance ) noexcept override
	{
		( void )nVertices;
		( void )nInstances;
		( void )nFirstVertex;
		( void )nFirstInstance;
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t drawIndexed( uint32_t nIndices, uint32_t nInstances, uint32_t nFirstIndex, int32_t nVertexOffset,
	                                 uint32_t nFirstInstance ) noexcept override
	{
		( void )nIndices;
		( void )nInstances;
		( void )nFirstIndex;
		( void )nVertexOffset;
		( void )nFirstInstance;
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t dispatch( uint32_t nX, uint32_t nY, uint32_t nZ ) noexcept override
	{
		( void )nX;
		( void )nY;
		( void )nZ;
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	sirGraphicsResult_t close( void ) noexcept override
	{
		return SIR_GRAPHICS_SUCCESS;
	}
};

static bool testRegistry( void ) noexcept
{
	SIRGraphicsRegistry registry;
	SIRGraphicsTestDriver vulkan( SIR_GRAPHICS_BACKEND_VULKAN, SIR_GRAPHICS_FEATURE_GRAPHICS | SIR_GRAPHICS_FEATURE_COMPUTE, 1, 4, false );
	SIRGraphicsTestDriver d3d12( SIR_GRAPHICS_BACKEND_D3D12, SIR_GRAPHICS_FEATURE_GRAPHICS, 12, 0, false );
	SIRGraphicsTestDriver ogl( SIR_GRAPHICS_BACKEND_OPENGL, SIR_GRAPHICS_FEATURE_GRAPHICS, 2, 1, true );
	SIRGraphicsTestDriver invalid( SIR_GRAPHICS_BACKEND_AUTO, SIR_GRAPHICS_FEATURE_GRAPHICS, 1, 0, false );

	if ( registry.getCapacity() == 0 || registry.getCount() != 0 || registry.registerDriver( nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	if ( registry.registerDriver( &invalid ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	if ( registry.registerDriver( &vulkan ) != SIR_GRAPHICS_SUCCESS || registry.registerDriver( &d3d12 ) != SIR_GRAPHICS_SUCCESS ||
	     registry.registerDriver( &ogl ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	if ( registry.getCount() != 3 || registry.registerDriver( &vulkan ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}

	SIRGraphicsDriver *pChosen = nullptr;
	sirGraphicsAdapterInfo_t adapter = {};
	sirGraphicsSelection_t selection = {};
	selection.eBackend = SIR_GRAPHICS_BACKEND_D3D12;
	selection.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	if ( registry.selectDriver( selection, &pChosen, &adapter ) != SIR_GRAPHICS_UNSUPPORTED || pChosen != nullptr ) {
		return false;
	}

	selection.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	selection.nMinimumApiVersion = { 12, 1, 0 };
	if ( registry.selectDriver( selection, &pChosen, &adapter ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	selection.nMinimumApiVersion = { 12, 0, 0 };
	if ( registry.selectDriver( selection, &pChosen, &adapter ) != SIR_GRAPHICS_SUCCESS || pChosen != &d3d12 ) {
		return false;
	}

	selection.eBackend = SIR_GRAPHICS_BACKEND_AUTO;
	selection.nMinimumApiVersion = {};
	selection.nRequiredFeatures = SIR_GRAPHICS_FEATURE_COMPUTE;
	if ( registry.selectDriver( selection, &pChosen, &adapter ) != SIR_GRAPHICS_SUCCESS || pChosen != &vulkan ) {
		return false;
	}
	selection.eBackend = SIR_GRAPHICS_BACKEND_OPENGL;
	selection.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	if ( registry.selectDriver( selection, &pChosen, &adapter ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	selection.bAllowSoftware = true;
	if ( registry.selectDriver( selection, &pChosen, &adapter ) != SIR_GRAPHICS_SUCCESS || pChosen != &ogl ) {
		return false;
	}

	ogl.setAdapterCount( 257 );
	if ( registry.selectDriver( selection, &pChosen, &adapter ) != SIR_GRAPHICS_CAPACITY_EXCEEDED ) {
		return false;
	}
	selection.eBackend = ( sirGraphicsBackend_t )99;
	if ( registry.selectDriver( selection, &pChosen, &adapter ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	if ( registry.selectDriver( selection, nullptr, &adapter ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}

	if ( registry.unregisterDriver( SIR_GRAPHICS_BACKEND_D3D12 ) != SIR_GRAPHICS_SUCCESS || registry.getCount() != 2 ||
	     registry.findDriver( SIR_GRAPHICS_BACKEND_D3D12 ) != nullptr ) {
		return false;
	}
	if ( registry.unregisterDriver( SIR_GRAPHICS_BACKEND_D3D12 ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	return registry.unregisterDriver( SIR_GRAPHICS_BACKEND_VULKAN ) == SIR_GRAPHICS_SUCCESS &&
	       registry.unregisterDriver( SIR_GRAPHICS_BACKEND_OPENGL ) == SIR_GRAPHICS_SUCCESS && registry.getCount() == 0;
}

static bool testRankedSelection( void ) noexcept
{
	SIRGraphicsRegistry registry;
	constexpr uint64_t GRAPHICS = SIR_GRAPHICS_FEATURE_GRAPHICS;
	constexpr uint64_t COMPUTE = SIR_GRAPHICS_FEATURE_COMPUTE;
	SIRGraphicsTestDriver software( SIR_GRAPHICS_BACKEND_OPENGL, GRAPHICS | COMPUTE | SIR_GRAPHICS_FEATURE_HDR_OUTPUT, 2, 1, true );
	SIRGraphicsTestDriver d3d12( SIR_GRAPHICS_BACKEND_D3D12, GRAPHICS | COMPUTE, 12, 0, false );
	SIRGraphicsTestDriver vulkan( SIR_GRAPHICS_BACKEND_VULKAN, GRAPHICS | COMPUTE, 1, 4, false );
	d3d12.setMemory( 2048, 0 );
	vulkan.setMemory( 4096, 0 );

	// Deliberately register software first and Vulkan last.
	if ( registry.registerDriver( &software ) != SIR_GRAPHICS_SUCCESS || registry.registerDriver( &d3d12 ) != SIR_GRAPHICS_SUCCESS ||
	     registry.registerDriver( &vulkan ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}

	sirGraphicsSelection_t selection = {};
	selection.eBackend = SIR_GRAPHICS_BACKEND_AUTO;
	selection.nRequiredFeatures = GRAPHICS;
	selection.bAllowSoftware = true;
	SIRGraphicsDriver *pDriver = nullptr;
	sirGraphicsAdapterInfo_t adapter = {};

	if ( registry.selectDriver( selection, &pDriver, &adapter ) != SIR_GRAPHICS_SUCCESS || pDriver != &vulkan ||
	     adapter.bSoftwareAdapter ) {
		return false;
	}
	// When dedicated memory differs but the feature set is the same, select
	// the eligible device with more reported dedicated memory.
	vulkan.setMemory( 1024, 0 );
	if ( registry.selectDriver( selection, &pDriver, &adapter ) != SIR_GRAPHICS_SUCCESS || pDriver != &d3d12 ) {
		return false;
	}
	// Deterministic backend tie-break is independent of registration order.
	vulkan.setMemory( 2048, 0 );
	if ( registry.selectDriver( selection, &pDriver, &adapter ) != SIR_GRAPHICS_SUCCESS || pDriver != &vulkan ) {
		return false;
	}

	// Shared system memory provides the next deterministic tie-break when
	// dedicated memory and capabilities are identical.
	vulkan.setMemory( 2048, 1024 );
	d3d12.setMemory( 2048, 512 );
	if ( registry.selectDriver( selection, &pDriver, &adapter ) != SIR_GRAPHICS_SUCCESS || pDriver != &vulkan ) {
		return false;
	}
	d3d12.setMemory( 2048, 2048 );
	if ( registry.selectDriver( selection, &pDriver, &adapter ) != SIR_GRAPHICS_SUCCESS || pDriver != &d3d12 ) {
		return false;
	}
	vulkan.setMemory( 2048, 0 );
	d3d12.setMemory( 2048, 0 );
	// Software is eligible only when the caller permits it, and only after
	// feature and version checks have excluded every physical candidate.
	selection.nRequiredFeatures = GRAPHICS | SIR_GRAPHICS_FEATURE_HDR_OUTPUT;
	if ( registry.selectDriver( selection, &pDriver, &adapter ) != SIR_GRAPHICS_SUCCESS || pDriver != &software ||
	     !adapter.bSoftwareAdapter ) {
		return false;
	}
	selection.bAllowSoftware = false;
	if ( registry.selectDriver( selection, &pDriver, &adapter ) != SIR_GRAPHICS_UNSUPPORTED || pDriver != nullptr ) {
		return false;
	}
	selection.bAllowSoftware = true;

	// Explicit backend selection remains strict; no cross-API software path.
	selection.eBackend = SIR_GRAPHICS_BACKEND_D3D12;
	if ( registry.selectDriver( selection, &pDriver, &adapter ) != SIR_GRAPHICS_UNSUPPORTED ) {
		return false;
	}
	selection.eBackend = SIR_GRAPHICS_BACKEND_OPENGL;
	if ( registry.selectDriver( selection, &pDriver, &adapter ) != SIR_GRAPHICS_SUCCESS || pDriver != &software ) {
		return false;
	}
	// API versions cannot be compared across different backend version systems.
	selection.eBackend = SIR_GRAPHICS_BACKEND_AUTO;
	selection.nMinimumApiVersion = { 1, 4, 0 };
	if ( registry.selectDriver( selection, &pDriver, &adapter ) != SIR_GRAPHICS_INVALID_ARGUMENT || pDriver != nullptr ) {
		return false;
	}
	selection.nMinimumApiVersion = {};

	SIRGraphicsRegistry invalidRegistry;
	SIRGraphicsTestDriver invalid( SIR_GRAPHICS_BACKEND_VULKAN, GRAPHICS, 0, 0, false );
	if ( invalidRegistry.registerDriver( &invalid ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	selection.eBackend = SIR_GRAPHICS_BACKEND_VULKAN;
	selection.nRequiredFeatures = GRAPHICS;
	if ( invalidRegistry.selectDriver( selection, &pDriver, &adapter ) != SIR_GRAPHICS_DRIVER_ERROR ) {
		return false;
	}
	SIRGraphicsRegistry invalidFeatures;
	SIRGraphicsTestDriver bogus( SIR_GRAPHICS_BACKEND_VULKAN, GRAPHICS | ( 1ULL << 63 ), 1, 4, false );
	if ( invalidFeatures.registerDriver( &bogus ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	return invalidFeatures.selectDriver( selection, &pDriver, &adapter ) == SIR_GRAPHICS_DRIVER_ERROR;
}

static bool testRegistryBackendFailurePolicy( void ) noexcept
{
	SIRGraphicsRegistry registry;
	SIRGraphicsTestDriver vulkan( SIR_GRAPHICS_BACKEND_VULKAN, SIR_GRAPHICS_FEATURE_GRAPHICS, 1, 4, false );
	SIRGraphicsTestDriver d3d12( SIR_GRAPHICS_BACKEND_D3D12, SIR_GRAPHICS_FEATURE_GRAPHICS, 12, 0, false );
	SIRGraphicsTestDriver invalid( ( sirGraphicsBackend_t )99, SIR_GRAPHICS_FEATURE_GRAPHICS, 1, 0, false );
	if ( registry.registerDriver( &vulkan ) != SIR_GRAPHICS_SUCCESS || registry.registerDriver( &d3d12 ) != SIR_GRAPHICS_SUCCESS ||
	     registry.registerDriver( &invalid ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	sirGraphicsSelection_t selection = {};
	selection.eBackend = SIR_GRAPHICS_BACKEND_AUTO;
	selection.nRequiredFeatures = SIR_GRAPHICS_FEATURE_GRAPHICS;
	SIRGraphicsDriver *pChosen = nullptr;
	sirGraphicsAdapterInfo_t adapter = {};
#define CHECK_REGISTRY( expected, label )                                                                                                  \
	if ( registry.selectDriver( selection, &pChosen, &adapter ) != expected ) {                                                            \
		fputs( "Registry error policy: " label "\n", stderr );                                                                             \
		return false;                                                                                                                      \
	}
	vulkan.setProviderResults( SIR_GRAPHICS_DEVICE_LOST, SIR_GRAPHICS_SUCCESS );
	CHECK_REGISTRY( SIR_GRAPHICS_SUCCESS, "AUTO skips failed adapter count" );
	if ( pChosen != &d3d12 ) {
		return false;
	}
	selection.eBackend = SIR_GRAPHICS_BACKEND_VULKAN;
	CHECK_REGISTRY( SIR_GRAPHICS_DEVICE_LOST, "explicit adapter count failure propagates" );
	selection.eBackend = SIR_GRAPHICS_BACKEND_AUTO;
	vulkan.setProviderResults( SIR_GRAPHICS_SUCCESS, SIR_GRAPHICS_DEVICE_LOST );
	CHECK_REGISTRY( SIR_GRAPHICS_SUCCESS, "AUTO skips failed adapter query" );
	if ( pChosen != &d3d12 ) {
		return false;
	}
	selection.eBackend = SIR_GRAPHICS_BACKEND_VULKAN;
	CHECK_REGISTRY( SIR_GRAPHICS_DEVICE_LOST, "explicit query failure propagates" );
	vulkan.setProviderResults( SIR_GRAPHICS_SUCCESS, SIR_GRAPHICS_SUCCESS );
	vulkan.setAdapterCount( 0 );
	selection.eBackend = SIR_GRAPHICS_BACKEND_AUTO;
	CHECK_REGISTRY( SIR_GRAPHICS_SUCCESS, "empty adapter provider skipped" );
	if ( pChosen != &d3d12 ) {
		return false;
	}
	vulkan.setAdapterCount( 1 );
	selection.nMinimumApiVersion = { 0, 0, 1 };
	CHECK_REGISTRY( SIR_GRAPHICS_INVALID_ARGUMENT, "AUTO forbids patch-level API minimum" );
	selection.eBackend = SIR_GRAPHICS_BACKEND_VULKAN;
	CHECK_REGISTRY( SIR_GRAPHICS_INVALID_ARGUMENT, "explicit minor without major forbidden" );
	selection.nMinimumApiVersion = { 1, 4, 1 };
	CHECK_REGISTRY( SIR_GRAPHICS_UNSUPPORTED, "patch version greater than adapter version" );
	selection.nMinimumApiVersion = { 1, 4, 0 };
	CHECK_REGISTRY( SIR_GRAPHICS_SUCCESS, "equal patch version succeeds" );
	selection.nMinimumApiVersion = { 1, 0, 0 };
	CHECK_REGISTRY( SIR_GRAPHICS_SUCCESS, "older minor version admitted" );
	selection.nMinimumApiVersion = { 2, 0, 0 };
	CHECK_REGISTRY( SIR_GRAPHICS_UNSUPPORTED, "greater API major rejected" );
	selection.nMinimumApiVersion = {};
	selection.eBackend = SIR_GRAPHICS_BACKEND_AUTO;
	if ( registry.selectDriver( selection, &pChosen, nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
		return false;
	}
	selection.nRequiredFeatures = SIR_GRAPHICS_FEATURE_RAY_TRACING;
	CHECK_REGISTRY( SIR_GRAPHICS_UNSUPPORTED, "feature floor higher than every adapter" );
#undef CHECK_REGISTRY
	return true;
}

static bool testHandlesAndCompletions( void ) noexcept
{
	sirGraphicsHandle_t handle = { 0, 1, 7, SIR_GRAPHICS_HANDLE_BUFFER };
	if ( !sirGraphicsIsHandleValid( handle, SIR_GRAPHICS_HANDLE_BUFFER, 7 ) ) {
		return false;
	}
	if ( sirGraphicsIsHandleValid( handle, SIR_GRAPHICS_HANDLE_TEXTURE, 7 ) ||
	     sirGraphicsIsHandleValid( handle, SIR_GRAPHICS_HANDLE_BUFFER, 6 ) ) {
		return false;
	}
	handle.nGeneration = 0;
	if ( sirGraphicsIsHandleValid( handle, SIR_GRAPHICS_HANDLE_BUFFER, 7 ) ) {
		return false;
	}

	sirGraphicsCompletionSet_t set = {};
	set.nDeviceGeneration = 7;
	if ( !sirGraphicsIsCompletionSetValid( set, 7 ) ) {
		return false;
	}
	set.nQueueMask = 1U << SIR_GRAPHICS_QUEUE_DIRECT;
	if ( sirGraphicsIsCompletionSetValid( set, 7 ) ) {
		return false;
	}
	set.szQueueValues[SIR_GRAPHICS_QUEUE_DIRECT] = 51;
	if ( !sirGraphicsIsCompletionSetValid( set, 7 ) || sirGraphicsIsCompletionSetValid( set, 6 ) ) {
		return false;
	}
	set.szQueueValues[SIR_GRAPHICS_QUEUE_COPY] = 1;
	if ( sirGraphicsIsCompletionSetValid( set, 7 ) ) {
		return false;
	}
	set.nQueueMask |= 1U << SIR_GRAPHICS_QUEUE_COPY;
	if ( !sirGraphicsIsCompletionSetValid( set, 7 ) ) {
		return false;
	}
	set.nQueueMask |= 1U << 9;
	return !sirGraphicsIsCompletionSetValid( set, 7 );
}

static bool testResourcesAndPipelines( void ) noexcept
{
	sirGraphicsBufferDesc_t buffer = {};
	buffer.nSizeBytes = 1024;
	buffer.nUsage = SIR_GRAPHICS_USAGE_VERTEX | SIR_GRAPHICS_USAGE_COPY_DESTINATION;
	buffer.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	if ( sirGraphicsValidateBuffer( buffer ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	buffer.nAlignmentBytes = 384;
	if ( sirGraphicsValidateBuffer( buffer ) == SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	buffer.nAlignmentBytes = 256;
	buffer.nUsage = SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	if ( sirGraphicsValidateBuffer( buffer ) == SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	buffer.nUsage = SIR_GRAPHICS_USAGE_VERTEX;
	buffer.nSizeBytes = 0;
	if ( sirGraphicsValidateBuffer( buffer ) == SIR_GRAPHICS_SUCCESS ) {
		return false;
	}

	sirGraphicsTextureDesc_t texture = {};
	texture.nWidth = 512;
	texture.nHeight = 512;
	texture.nDepth = 1;
	texture.nArrayLayers = 1;
	texture.nMipLevels = 1;
	texture.nSampleCount = 1;
	texture.nUsage = SIR_GRAPHICS_USAGE_COLOR_ATTACHMENT;
	texture.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	texture.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	texture.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	if ( sirGraphicsValidateTexture( texture ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	texture.nSampleCount = 3;
	if ( sirGraphicsValidateTexture( texture ) == SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	texture.nSampleCount = 1;
	texture.eDimension = SIR_GRAPHICS_TEXTURE_CUBE;
	texture.nArrayLayers = 6;
	if ( sirGraphicsValidateTexture( texture ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	texture.nHeight = 256;
	if ( sirGraphicsValidateTexture( texture ) == SIR_GRAPHICS_SUCCESS ) {
		return false;
	}

	sirGraphicsSamplerDesc_t sampler = {};
	sampler.eFilter = SIR_GRAPHICS_FILTER_ANISOTROPIC;
	sampler.eAddressU = SIR_GRAPHICS_ADDRESS_REPEAT;
	sampler.eAddressV = SIR_GRAPHICS_ADDRESS_CLAMP;
	sampler.eAddressW = SIR_GRAPHICS_ADDRESS_BORDER;
	sampler.fMaximumAnisotropy = 8.0f;
	sampler.fMaximumLod = 16.0f;
	if ( sirGraphicsValidateSampler( sampler ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	sampler.eFilter = ( sirGraphicsSamplerFilter_t )99;
	if ( sirGraphicsValidateSampler( sampler ) == SIR_GRAPHICS_SUCCESS ) {
		return false;
	}

	const uint32_t szFakeShader[] = { 0x07230203, 0, 0, 0 };
	sirGraphicsABIModernRecipe_t modern = {};
	modern.nSize = sizeof( modern );
	modern.nStageMask = SIR_GRAPHICS_STAGE_COMPUTE;
	sirGraphicsPipelineDesc_t pipeline = {};
	pipeline.eType = SIR_GRAPHICS_PIPELINE_COMPUTE;
	pipeline.eModel = SIR_GRAPHICS_PIPELINE_MODEL_MODERN;
	pipeline.pModern = &modern;
	pipeline.nSampleCount = 1;
	pipeline.computeShader = { SIR_GRAPHICS_SHADER_SPIRV, szFakeShader, sizeof( szFakeShader ), 1, "main" };
	if ( sirGraphicsValidatePipeline( pipeline ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	pipeline.nAttributeCount = 1;
	if ( sirGraphicsValidatePipeline( pipeline ) == SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	pipeline.nAttributeCount = 0;
	pipeline.eType = SIR_GRAPHICS_PIPELINE_GRAPHICS;
	modern.nStageMask = SIR_GRAPHICS_STAGE_VERTEX | SIR_GRAPHICS_STAGE_FRAGMENT;
	pipeline.computeShader = {};
	pipeline.vertexShader = { SIR_GRAPHICS_SHADER_SPIRV, szFakeShader, sizeof( szFakeShader ), 2, "main" };
	pipeline.pixelShader = { SIR_GRAPHICS_SHADER_DXIL, szFakeShader, sizeof( szFakeShader ), 3, "main" };
	pipeline.nSampleCount = 1;
	pipeline.nColorFormatCount = 1;
	pipeline.szColorFormats[0] = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	if ( sirGraphicsValidatePipeline( pipeline ) == SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	pipeline.pixelShader.eFormat = SIR_GRAPHICS_SHADER_SPIRV;
	pipeline.eTopology = SIR_GRAPHICS_TOPOLOGY_TRIANGLES;
	pipeline.eCullMode = SIR_GRAPHICS_CULL_BACK;
	pipeline.eDepthCompare = SIR_GRAPHICS_COMPARE_LESS_EQUAL;
	if ( sirGraphicsValidatePipeline( pipeline ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	pipeline.eTopology = ( sirGraphicsTopology_t )99;
	if ( sirGraphicsValidatePipeline( pipeline ) == SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	pipeline.eTopology = SIR_GRAPHICS_TOPOLOGY_TRIANGLES;
	pipeline.eType = SIR_GRAPHICS_PIPELINE_FIXED_FUNCTION;
	pipeline.vertexShader = {};
	pipeline.pixelShader = {};
	sirGraphicsABIFixedRecipe_t fixed = {};
	fixed.nSize = sizeof( fixed );
	pipeline.eModel = SIR_GRAPHICS_PIPELINE_MODEL_FIXED_FUNCTION;
	pipeline.pModern = nullptr;
	pipeline.pFixed = &fixed;
	return sirGraphicsValidatePipeline( pipeline ) == SIR_GRAPHICS_SUCCESS;
}

static bool testSubmissions( void ) noexcept
{
	SIRGraphicsTestCommandList direct( SIR_GRAPHICS_QUEUE_DIRECT );
	SIRGraphicsTestCommandList compute( SIR_GRAPHICS_QUEUE_COMPUTE );
	SIRGraphicsCommandList *szLists[] = { &direct };
	sirGraphicsCompletion_t wait = { 27, SIR_GRAPHICS_QUEUE_COPY, 55 };
	sirGraphicsSubmission_t submission = { szLists, &wait, nullptr, 1, 1, 0 };
	if ( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_DIRECT, submission, 27 ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	if ( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_COMPUTE, submission, 27 ) == SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	szLists[0] = &compute;
	if ( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_COMPUTE, submission, 27 ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	wait.nDeviceGeneration = 28;
	if ( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_COMPUTE, submission, 27 ) == SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	wait.nDeviceGeneration = 27;
	wait.nValue = 0;
	if ( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_COMPUTE, submission, 27 ) == SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	wait.nValue = 55;
	sirGraphicsSurfaceImage_t image = {};
	image.hSurface = { 0, 1, 27, SIR_GRAPHICS_HANDLE_SURFACE };
	image.hImage = { 1, 1, 27, SIR_GRAPHICS_HANDLE_TEXTURE };
	image.nAcquisitionId = 77;
	image.nWindowGeneration = 5;
	submission.pAcquisitions = &image;
	submission.nAcquisitionCount = 1;
	if ( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_COMPUTE, submission, 27 ) != SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	image.nAcquisitionId = 0;
	if ( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_COMPUTE, submission, 27 ) == SIR_GRAPHICS_SUCCESS ) {
		return false;
	}
	image.nAcquisitionId = 77;
	submission.nCommandListCount = 0;
	return sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_COMPUTE, submission, 27 ) == SIR_GRAPHICS_INVALID_ARGUMENT;
}

static bool testSubmissionBoundaryMatrix( void ) noexcept
{
	SIRGraphicsTestCommandList direct( SIR_GRAPHICS_QUEUE_DIRECT );
	SIRGraphicsTestCommandList compute( SIR_GRAPHICS_QUEUE_COMPUTE );
	SIRGraphicsCommandList *szLists[] = { &direct };
	sirGraphicsCompletion_t wait = { 27, SIR_GRAPHICS_QUEUE_DIRECT, 1 };
	sirGraphicsSurfaceImage_t image = {};
	image.hSurface = { 0, 1, 27, SIR_GRAPHICS_HANDLE_SURFACE };
	image.hImage = { 1, 1, 27, SIR_GRAPHICS_HANDLE_TEXTURE };
	image.nAcquisitionId = 1;
	image.nWindowGeneration = 1;
	sirGraphicsSubmission_t base = { szLists, &wait, &image, 1, 1, 1 };
#define CHECK_SUBMISSION_BOUNDARY( observed, expected, label )                                                                             \
	if ( ( observed ) != ( expected ) ) {                                                                                                  \
		fputs( "Invalid source submission: " label "\n", stderr );                                                                         \
		return false;                                                                                                                      \
	}
	CHECK_SUBMISSION_BOUNDARY( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_DIRECT, base, 27 ), SIR_GRAPHICS_SUCCESS,
	                           "valid baseline" );
	CHECK_SUBMISSION_BOUNDARY( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_COUNT, base, 27 ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                           "invalid queue enum" );
	CHECK_SUBMISSION_BOUNDARY( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_DIRECT, base, 0 ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                           "missing generation" );
	sirGraphicsSubmission_t test = base;
	test.nCommandListCount = 0;
	CHECK_SUBMISSION_BOUNDARY( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_DIRECT, test, 27 ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                           "zero command lists" );
	test = base;
	test.nCommandListCount = 65;
	CHECK_SUBMISSION_BOUNDARY( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_DIRECT, test, 27 ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                           "overfull command list batch" );
	test = base;
	test.ppCommandLists = nullptr;
	CHECK_SUBMISSION_BOUNDARY( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_DIRECT, test, 27 ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                           "null command pointer list" );
	test = base;
	test.nWaitCount = 65;
	CHECK_SUBMISSION_BOUNDARY( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_DIRECT, test, 27 ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                           "overfull wait batch" );
	test = base;
	test.pWaits = nullptr;
	CHECK_SUBMISSION_BOUNDARY( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_DIRECT, test, 27 ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                           "null waits" );
	test = base;
	test.nAcquisitionCount = 9;
	CHECK_SUBMISSION_BOUNDARY( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_DIRECT, test, 27 ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                           "overfull acquisitions" );
	test = base;
	test.pAcquisitions = nullptr;
	CHECK_SUBMISSION_BOUNDARY( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_DIRECT, test, 27 ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                           "null acquisitions" );
	SIRGraphicsCommandList *szNull[] = { nullptr };
	test = base;
	test.ppCommandLists = szNull;
	CHECK_SUBMISSION_BOUNDARY( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_DIRECT, test, 27 ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                           "null command" );
	SIRGraphicsCommandList *szWrong[] = { &compute };
	test = base;
	test.ppCommandLists = szWrong;
	CHECK_SUBMISSION_BOUNDARY( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_DIRECT, test, 27 ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                           "wrong command queue" );
	wait.nDeviceGeneration = 28;
	CHECK_SUBMISSION_BOUNDARY( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_DIRECT, base, 27 ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                           "stale wait token" );
	wait.nDeviceGeneration = 27;
	wait.eQueue = SIR_GRAPHICS_QUEUE_COUNT;
	CHECK_SUBMISSION_BOUNDARY( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_DIRECT, base, 27 ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                           "invalid wait queue" );
	wait.eQueue = SIR_GRAPHICS_QUEUE_DIRECT;
	wait.nValue = 0;
	CHECK_SUBMISSION_BOUNDARY( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_DIRECT, base, 27 ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                           "zero wait value" );
	wait.nValue = 1;
	image.hSurface.eType = SIR_GRAPHICS_HANDLE_TEXTURE;
	CHECK_SUBMISSION_BOUNDARY( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_DIRECT, base, 27 ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                           "wrong surface type" );
	image.hSurface.eType = SIR_GRAPHICS_HANDLE_SURFACE;
	image.hSurface.nDeviceGeneration = 28;
	CHECK_SUBMISSION_BOUNDARY( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_DIRECT, base, 27 ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                           "stale surface" );
	image.hSurface.nDeviceGeneration = 27;
	image.hImage.eType = SIR_GRAPHICS_HANDLE_BUFFER;
	CHECK_SUBMISSION_BOUNDARY( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_DIRECT, base, 27 ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                           "wrong image type" );
	image.hImage.eType = SIR_GRAPHICS_HANDLE_TEXTURE;
	image.hImage.nDeviceGeneration = 28;
	CHECK_SUBMISSION_BOUNDARY( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_DIRECT, base, 27 ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                           "stale image" );
	image.hImage.nDeviceGeneration = 27;
	image.nAcquisitionId = 0;
	CHECK_SUBMISSION_BOUNDARY( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_DIRECT, base, 27 ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                           "zero acquisition identity" );
	image.nAcquisitionId = 1;
	image.nWindowGeneration = 0;
	CHECK_SUBMISSION_BOUNDARY( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_DIRECT, base, 27 ), SIR_GRAPHICS_INVALID_ARGUMENT,
	                           "zero window generation" );
	image.nWindowGeneration = 1;
	CHECK_SUBMISSION_BOUNDARY( sirGraphicsValidateSubmission( SIR_GRAPHICS_QUEUE_DIRECT, base, 27 ), SIR_GRAPHICS_SUCCESS,
	                           "restored valid multi-queue wait and WSI" );
#undef CHECK_SUBMISSION_BOUNDARY
	return true;
}

static bool testSourceResourceBoundaryMatrix( void ) noexcept
{
	sirGraphicsBufferDesc_t buffer = {};
	buffer.nSizeBytes = 256;
	buffer.nUsage = SIR_GRAPHICS_USAGE_COPY_SOURCE;
	buffer.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
#define CHECK_RESOURCE( observed, expected, label )                                                                                        \
	if ( ( observed ) != ( expected ) ) {                                                                                                  \
		fputs( "Source resource boundary: " label "\n", stderr );                                                                          \
		return false;                                                                                                                      \
	}
	CHECK_RESOURCE( sirGraphicsValidateBuffer( buffer ), SIR_GRAPHICS_SUCCESS, "valid buffer" );
	buffer.nSizeBytes = 0;
	CHECK_RESOURCE( sirGraphicsValidateBuffer( buffer ), SIR_GRAPHICS_INVALID_ARGUMENT, "zero byte buffer" );
	buffer.nSizeBytes = 256;
	buffer.nUsage = 0;
	CHECK_RESOURCE( sirGraphicsValidateBuffer( buffer ), SIR_GRAPHICS_INVALID_ARGUMENT, "buffer without usage" );
	buffer.nUsage = SIR_GRAPHICS_USAGE_DEPTH_STENCIL;
	CHECK_RESOURCE( sirGraphicsValidateBuffer( buffer ), SIR_GRAPHICS_INVALID_ARGUMENT, "depth attachment buffer" );
	buffer.nUsage = SIR_GRAPHICS_USAGE_COPY_SOURCE;
	buffer.eMemory = ( sirGraphicsMemoryClass_t )( SIR_GRAPHICS_MEMORY_TRANSIENT + 1 );
	CHECK_RESOURCE( sirGraphicsValidateBuffer( buffer ), SIR_GRAPHICS_INVALID_ARGUMENT, "unrecognized buffer memory" );
	buffer.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	buffer.nAlignmentBytes = 17;
	CHECK_RESOURCE( sirGraphicsValidateBuffer( buffer ), SIR_GRAPHICS_INVALID_ARGUMENT, "non-power-of-two alignment" );
	buffer.nAlignmentBytes = 16;
	CHECK_RESOURCE( sirGraphicsValidateBuffer( buffer ), SIR_GRAPHICS_SUCCESS, "valid aligned buffer" );
	sirGraphicsTextureDesc_t texture = {};
	texture.nWidth = 64;
	texture.nHeight = 64;
	texture.nDepth = 1;
	texture.nArrayLayers = 1;
	texture.nMipLevels = 1;
	texture.nSampleCount = 1;
	texture.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	texture.eDimension = SIR_GRAPHICS_TEXTURE_2D;
	texture.eFormat = SIR_GRAPHICS_FORMAT_RGBA8_UNORM;
	texture.eMemory = SIR_GRAPHICS_MEMORY_GPU_LOCAL;
	CHECK_RESOURCE( sirGraphicsValidateTexture( texture ), SIR_GRAPHICS_SUCCESS, "valid texture" );
	texture.nWidth = 0;
	CHECK_RESOURCE( sirGraphicsValidateTexture( texture ), SIR_GRAPHICS_INVALID_ARGUMENT, "zero width" );
	texture.nWidth = 64;
	texture.nArrayLayers = 0;
	CHECK_RESOURCE( sirGraphicsValidateTexture( texture ), SIR_GRAPHICS_INVALID_ARGUMENT, "zero array layers" );
	texture.nArrayLayers = 1;
	texture.nSampleCount = 3;
	CHECK_RESOURCE( sirGraphicsValidateTexture( texture ), SIR_GRAPHICS_INVALID_ARGUMENT, "invalid sample count" );
	texture.nSampleCount = 1;
	texture.nUsage = 0;
	CHECK_RESOURCE( sirGraphicsValidateTexture( texture ), SIR_GRAPHICS_INVALID_ARGUMENT, "missing texture usage" );
	texture.nUsage = SIR_GRAPHICS_USAGE_SAMPLED;
	texture.eDimension = SIR_GRAPHICS_TEXTURE_1D;
	CHECK_RESOURCE( sirGraphicsValidateTexture( texture ), SIR_GRAPHICS_INVALID_ARGUMENT, "nonunit 1D height" );
	texture.eDimension = SIR_GRAPHICS_TEXTURE_3D;
	texture.nArrayLayers = 2;
	CHECK_RESOURCE( sirGraphicsValidateTexture( texture ), SIR_GRAPHICS_INVALID_ARGUMENT, "3D array disallowed" );
	texture.nArrayLayers = 1;
	texture.eDimension = SIR_GRAPHICS_TEXTURE_CUBE;
	CHECK_RESOURCE( sirGraphicsValidateTexture( texture ), SIR_GRAPHICS_INVALID_ARGUMENT, "incomplete cube array" );
	texture.nArrayLayers = 6;
	CHECK_RESOURCE( sirGraphicsValidateTexture( texture ), SIR_GRAPHICS_SUCCESS, "valid cube" );
	texture.nDepth = 2;
	CHECK_RESOURCE( sirGraphicsValidateTexture( texture ), SIR_GRAPHICS_INVALID_ARGUMENT, "nonunit cube depth" );
	sirGraphicsSamplerDesc_t sampler = {};
	sampler.fMaximumAnisotropy = 1.0f;
	sampler.fMinimumLod = 0.0f;
	sampler.fMaximumLod = 8.0f;
	CHECK_RESOURCE( sirGraphicsValidateSampler( sampler ), SIR_GRAPHICS_SUCCESS, "valid sampler" );
	sampler.eFilter = ( sirGraphicsSamplerFilter_t )( SIR_GRAPHICS_FILTER_ANISOTROPIC + 1 );
	CHECK_RESOURCE( sirGraphicsValidateSampler( sampler ), SIR_GRAPHICS_INVALID_ARGUMENT, "unknown filter" );
	sampler.eFilter = SIR_GRAPHICS_FILTER_LINEAR;
	sampler.eAddressV = ( sirGraphicsAddressMode_t )( SIR_GRAPHICS_ADDRESS_BORDER + 1 );
	CHECK_RESOURCE( sirGraphicsValidateSampler( sampler ), SIR_GRAPHICS_INVALID_ARGUMENT, "unknown V addressing" );
	sampler.eAddressV = SIR_GRAPHICS_ADDRESS_CLAMP;
	sampler.fMaximumAnisotropy = 17;
	CHECK_RESOURCE( sirGraphicsValidateSampler( sampler ), SIR_GRAPHICS_INVALID_ARGUMENT, "anisotropy overflow" );
	sampler.fMaximumAnisotropy = 1;
	sampler.fMinimumLod = 9;
	CHECK_RESOURCE( sirGraphicsValidateSampler( sampler ), SIR_GRAPHICS_INVALID_ARGUMENT, "min LOD above max LOD" );
#undef CHECK_RESOURCE
	return true;
}

int main( void )
{
	if ( !testRegistry() ) {
		fputs( "graphics.registry FAILED\n", stderr );
		return 1;
	}
	if ( !testRankedSelection() ) {
		fputs( "graphics.registry.ranking FAILED\n", stderr );
		return 5;
	}
	if ( !testRegistryBackendFailurePolicy() ) {
		fputs( "graphics.registry.failure-policy FAILED\n", stderr );
		return 7;
	}
	if ( !testHandlesAndCompletions() ) {
		fputs( "graphics.handles FAILED\n", stderr );
		return 2;
	}
	if ( !testResourcesAndPipelines() ) {
		fputs( "graphics.descriptors FAILED\n", stderr );
		return 3;
	}
	if ( !testSubmissionBoundaryMatrix() || !testSourceResourceBoundaryMatrix() ) {
		fputs( "graphics.boundaries FAILED\n", stderr );
		return 6;
	}
	if ( !testSubmissions() ) {
		fputs( "graphics.submissions FAILED\n", stderr );
		return 4;
	}
	puts( "PASS: shared graphics driver selection, ABI and negative contracts" );
	return 0;
}
