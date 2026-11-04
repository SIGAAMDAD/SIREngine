#include <Renderer/SIRGraphicsRegistry.h>
#include <Renderer/SIRGraphicsValidation.h>

#include <stdio.h>

class SIRGraphicsTestDriver final : public SIRGraphicsDriver
{
private:
	sirGraphicsBackend_t m_eBackend;
	sirGraphicsAdapterInfo_t m_Adapter = {};
	uint32_t m_nAdapterCount = 1;

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

	sirGraphicsBackend_t getBackend( void ) const noexcept override
	{
		return m_eBackend;
	}

	sirGraphicsResult_t getAdapterCount( uint32_t *pCount ) const noexcept override
	{
		if ( pCount == nullptr ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		*pCount = m_nAdapterCount;
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t queryAdapter( uint32_t nAdapterIndex, sirGraphicsAdapterInfo_t *pInfo ) const noexcept override
	{
		if ( pInfo == nullptr || nAdapterIndex >= m_nAdapterCount ) {
			return SIR_GRAPHICS_INVALID_ARGUMENT;
		}
		*pInfo = m_Adapter;
		pInfo->nAdapterIndex = nAdapterIndex;
		return SIR_GRAPHICS_SUCCESS;
	}

	sirGraphicsResult_t createDevice( const sirGraphicsDeviceCreateDesc_t& desc, SIRRenderingDevice **ppDevice ) noexcept override
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

	sirGraphicsResult_t copyBufferTexture( const sirGraphicsBufferTextureCopy_t& copy ) noexcept override
	{
		( void )copy;
		return SIR_GRAPHICS_UNSUPPORTED;
	}

	sirGraphicsResult_t beginRenderPass( const sirGraphicsRenderPass_t& pass ) noexcept override
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

	sirGraphicsResult_t setViewport( const sirGraphicsViewport_t& viewport, const sirGraphicsRect_t& scissor ) noexcept override
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

	if ( registry.getCount() != 0 || registry.registerDriver( nullptr ) != SIR_GRAPHICS_INVALID_ARGUMENT ) {
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
	sirGraphicsPipelineDesc_t pipeline = {};
	pipeline.eType = SIR_GRAPHICS_PIPELINE_COMPUTE;
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

int main( void )
{
	if ( !testRegistry() ) {
		fputs( "graphics.registry FAILED\n", stderr );
		return 1;
	}
	if ( !testHandlesAndCompletions() ) {
		fputs( "graphics.handles FAILED\n", stderr );
		return 2;
	}
	if ( !testResourcesAndPipelines() ) {
		fputs( "graphics.descriptors FAILED\n", stderr );
		return 3;
	}
	if ( !testSubmissions() ) {
		fputs( "graphics.submissions FAILED\n", stderr );
		return 4;
	}
	puts( "PASS: shared graphics driver selection, ABI and negative contracts" );
	return 0;
}
