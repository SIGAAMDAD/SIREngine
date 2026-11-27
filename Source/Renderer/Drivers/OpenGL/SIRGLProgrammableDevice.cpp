#include "SIRGLProgrammableDevice.h"
#include "SIRGLResources.h"
#include "SIRGLSceneFixtureRenderer.h"

#include <algorithm>
#include <cstdlib>
#include <new>

#if defined( _WIN32 )
#include <malloc.h>
#endif

static void *sirGLFactoryAllocate( void *, size_t nBytes, size_t nAlignment ) noexcept
{
	if ( nBytes == 0 || nAlignment == 0 ) return nullptr;
#if defined( _WIN32 )
	return _aligned_malloc( nBytes, nAlignment );
#else
	const size_t nRounded = ( nBytes + nAlignment - 1U ) / nAlignment * nAlignment;
	return std::aligned_alloc( nAlignment, nRounded );
#endif
}

static void sirGLFactoryDeallocate( void *, void *pMemory ) noexcept
{
#if defined( _WIN32 )
	_aligned_free( pMemory );
#else
	std::free( pMemory );
#endif
}

static const sirSharedAllocationCallbacks_t& sirGLFactoryCallbacks() noexcept
{
	static const sirSharedAllocationCallbacks_t callbacks{ nullptr, sirGLFactoryAllocate, sirGLFactoryDeallocate };
	return callbacks;
}

static bool sirGLFactoryOwner( const SIRSharedPtr<SIRRenderDevice>& owner, SIRGLProgrammableDevice& device ) noexcept
{
	return owner.get() == &device && device.isReady();
}

static SIRRenderCapabilities sirGLCapabilities( void ) noexcept
{
	SIRRenderCapabilities capabilities{};
	capabilities.nFeatureMask = static_cast<uint64_t>( SIRRenderFeature::SIR_RENDER_FEATURE_GRAPHICS ) |
	                            static_cast<uint64_t>( SIRRenderFeature::SIR_RENDER_FEATURE_MULTIPLE_RENDER_TARGETS ) |
	                            static_cast<uint64_t>( SIRRenderFeature::SIR_RENDER_FEATURE_PRESENT );
	capabilities.nMaximumTextureDimension = 16384;
	capabilities.nMaximumColorAttachments = 8;
	capabilities.nMaximumFramesInFlight = 1;
	capabilities.nMaximumBindingSlots = 36;
	capabilities.nMaximumSampleCount = 4;
	capabilities.nQueueMask = 1;
	capabilities.bProgrammableShaders = true;
	return capabilities;
}

SIRGLProgrammableDevice::SIRGLProgrammableDevice( void ) noexcept
    : SIRRenderDevice( SIRRenderBackend::SIR_RENDER_BACKEND_OPENGL_PROGRAMMABLE, sirGLCapabilities() ),
      m_Queue( *this ),
      m_Allocator( *this )
{
}

SIRGLProgrammableDevice::~SIRGLProgrammableDevice( void ) noexcept
{
	static_cast<void>( shutdown() );
}

SIRRenderResult SIRGLProgrammableDevice::initialize( void ) noexcept
{
	return initializeForWindow( SIRRenderNativeSurface{} );
}

SIRRenderResult SIRGLProgrammableDevice::initializeForWindow( const SIRRenderNativeSurface& native ) noexcept
{
	if ( m_pContext != nullptr ) return SIRRenderResult::SIR_RENDER_BUSY;
	const SIRRenderResult result = native.ePlatform == SIRRenderSurfacePlatform::SIR_RENDER_SURFACE_NONE
		? sirGLCreateNativeContext( &m_pContext )
		: sirGLCreateNativeContextForWindow( &native, &m_pContext );
	if ( result != SIRRenderResult::SIR_RENDER_SUCCESS ) return result;
	if ( sirGLGetContextProbe( m_pContext, &m_Probe ) != SIRRenderResult::SIR_RENDER_SUCCESS ||
	     !sirGLLoadProcs( sirGLResolveNativeProcedure, m_pContext, &m_Procs ) ) {
		static_cast<void>( shutdown() );
		return SIRRenderResult::SIR_RENDER_UNSUPPORTED;
	}
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRRenderQueue *SIRGLProgrammableDevice::getGraphicsQueue( void ) noexcept { return &m_Queue; }
SIRRenderQueue *SIRGLProgrammableDevice::getComputeQueue( void ) noexcept { return nullptr; }
SIRRenderQueue *SIRGLProgrammableDevice::getTransferQueue( void ) noexcept { return nullptr; }
SIRRenderMemoryAllocator& SIRGLProgrammableDevice::getMemoryAllocator( void ) noexcept { return m_Allocator; }

SIRRenderResult SIRGLProgrammableDevice::waitIdle( void ) noexcept
{
	if ( !isReady() ) return SIRRenderResult::SIR_RENDER_DEVICE_LOST;
	return m_Queue.waitIdle();
}

SIRRenderResult SIRGLProgrammableDevice::shutdown( void ) noexcept
{
	if ( m_pContext == nullptr ) return SIRRenderResult::SIR_RENDER_SUCCESS;
	if ( sirGLContextOwnsThread( m_pContext ) ) {
		static_cast<void>( waitIdle() );
		m_pEditorScene.reset();
		sirGLDestroyNativeContext( m_pContext );
		m_pContext = nullptr;
		m_Procs = {};
		m_Probe = {};
		m_bLost = false;
		return SIRRenderResult::SIR_RENDER_SUCCESS;
	}
	return SIRRenderResult::SIR_RENDER_BUSY;
}

SIRRenderResult SIRGLProgrammableDevice::renderEditorScene( uint32_t nWidth, uint32_t nHeight, float fLeftOffset, float fRightOffset ) noexcept
{
	if ( !isReady() || nWidth == 0 || nHeight == 0 ) return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	if ( !m_pEditorScene ) {
		m_pEditorScene = std::unique_ptr<SIRGLSceneFixtureRenderer>( new ( std::nothrow ) SIRGLSceneFixtureRenderer( m_Procs ) );
		if ( !m_pEditorScene || !m_pEditorScene->initialize() ) {
			m_pEditorScene.reset();
			return SIRRenderResult::SIR_RENDER_DRIVER_ERROR;
		}
	}
	return m_pEditorScene->render( nWidth, nHeight, fLeftOffset, fRightOffset ) ? SIRRenderResult::SIR_RENDER_SUCCESS : SIRRenderResult::SIR_RENDER_DRIVER_ERROR;
}

GLuint SIRGLProgrammableDevice::getEditorSceneTexture( void ) const noexcept
{
	return m_pEditorScene ? m_pEditorScene->getColorTexture() : 0;
}

bool SIRGLProgrammableDevice::isLost( void ) const noexcept
{
	return m_bLost || m_pContext == nullptr;
}

SIRRenderResult SIRGLProgrammableDevice::createPresentWindow( const SIRRenderNativeSurface& native,
                                                               const SIRRenderSwapchainDesc& description,
                                                               PresentWindow *pWindow ) noexcept
{
	if ( !isReady() || pWindow == nullptr || pWindow->active ) return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	const SIRRenderResult result = sirGLCreatePresentWindow( m_pContext, &native, &description, &pWindow->native );
	if ( result == SIRRenderResult::SIR_RENDER_SUCCESS ) pWindow->active = true;
	return result;
}

SIRRenderResult SIRGLProgrammableDevice::resizePresentWindow( PresentWindow *pWindow, uint32_t nWidth, uint32_t nHeight ) noexcept
{
	if ( !isReady() || pWindow == nullptr || !pWindow->active ) return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	return sirGLResizePresentWindow( m_pContext, &pWindow->native, nWidth, nHeight );
}

SIRRenderResult SIRGLProgrammableDevice::beginPresentWindow( PresentWindow *pWindow ) noexcept
{
	if ( !isReady() || pWindow == nullptr || !pWindow->active ) return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	return sirGLBeginPresentWindow( m_pContext, &pWindow->native );
}

SIRRenderResult SIRGLProgrammableDevice::swapPresentWindow( PresentWindow *pWindow ) noexcept
{
	if ( !isReady() || pWindow == nullptr || !pWindow->active ) return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	return sirGLSwapPresentWindow( m_pContext, &pWindow->native );
}

SIRRenderResult SIRGLProgrammableDevice::destroyPresentWindow( PresentWindow *pWindow ) noexcept
{
	if ( pWindow == nullptr || !pWindow->active ) return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	const SIRRenderResult result = sirGLDestroyPresentWindow( m_pContext, &pWindow->native );
	if ( result == SIRRenderResult::SIR_RENDER_SUCCESS ) pWindow->active = false;
	return result;
}

SIRSharedPtr<SIRGLStorageBuffer> SIRGLProgrammableDevice::createStorageBuffer( const SIRSharedPtr<SIRRenderDevice>& owner, const SIRRenderBufferDesc& description ) noexcept
{
	return sirGLFactoryOwner( owner, *this ) ? SIRMakeShared<SIRGLStorageBuffer>( sirGLFactoryCallbacks(), owner, description ) : SIRSharedPtr<SIRGLStorageBuffer>();
}

SIRSharedPtr<SIRGLTexture> SIRGLProgrammableDevice::createTexture( const SIRSharedPtr<SIRRenderDevice>& owner, const SIRRenderTextureDesc& description ) noexcept
{
	return sirGLFactoryOwner( owner, *this ) ? SIRMakeShared<SIRGLTexture>( sirGLFactoryCallbacks(), owner, description ) : SIRSharedPtr<SIRGLTexture>();
}

SIRSharedPtr<SIRGLShader> SIRGLProgrammableDevice::createShader( const SIRSharedPtr<SIRRenderDevice>& owner, const SIRRenderShaderCode& description ) noexcept
{
	return sirGLFactoryOwner( owner, *this ) ? SIRMakeShared<SIRGLShader>( sirGLFactoryCallbacks(), owner, description ) : SIRSharedPtr<SIRGLShader>();
}

SIRSharedPtr<SIRGLProgram> SIRGLProgrammableDevice::createProgram( const SIRSharedPtr<SIRRenderDevice>& owner, SIRSpan<const SIRSharedPtr<SIRGLShader>> shaders ) noexcept
{
	return sirGLFactoryOwner( owner, *this ) ? SIRMakeShared<SIRGLProgram>( sirGLFactoryCallbacks(), owner, shaders ) : SIRSharedPtr<SIRGLProgram>();
}

SIRSharedPtr<SIRGLBindingLayout> SIRGLProgrammableDevice::createBindingLayout( const SIRSharedPtr<SIRRenderDevice>& owner, SIRSpan<const SIRRenderBindingDesc> bindings ) noexcept
{
	return sirGLFactoryOwner( owner, *this ) ? SIRMakeShared<SIRGLBindingLayout>( sirGLFactoryCallbacks(), owner, bindings ) : SIRSharedPtr<SIRGLBindingLayout>();
}

SIRSharedPtr<SIRGLBindingSet> SIRGLProgrammableDevice::createBindingSet( const SIRSharedPtr<SIRRenderDevice>& owner, const SIRSharedPtr<SIRRenderBindingLayout>& layout, SIRSpan<const SIRRenderBindingDesc> bindings ) noexcept
{
	return sirGLFactoryOwner( owner, *this ) ? SIRMakeShared<SIRGLBindingSet>( sirGLFactoryCallbacks(), owner, layout, bindings ) : SIRSharedPtr<SIRGLBindingSet>();
}

SIRSharedPtr<SIRGLPipeline> SIRGLProgrammableDevice::createPipeline( const SIRSharedPtr<SIRRenderDevice>& owner, const SIRSharedPtr<SIRGLBindingLayout>& layout, const SIRSharedPtr<SIRGLProgram>& program, const SIRRenderPipelineDesc& description ) noexcept
{
	return sirGLFactoryOwner( owner, *this ) ? SIRMakeShared<SIRGLPipeline>( sirGLFactoryCallbacks(), owner, layout, program, description ) : SIRSharedPtr<SIRGLPipeline>();
}

SIRSharedPtr<SIRGLFramebuffer> SIRGLProgrammableDevice::createFramebuffer( const SIRSharedPtr<SIRRenderDevice>& owner, const SIRRenderFramebufferDesc& description, const SIRSharedPtr<SIRGLTexture>& color, const SIRSharedPtr<SIRGLTexture>& depth ) noexcept
{
	return sirGLFactoryOwner( owner, *this ) ? SIRMakeShared<SIRGLFramebuffer>( sirGLFactoryCallbacks(), owner, description, color, depth ) : SIRSharedPtr<SIRGLFramebuffer>();
}

SIRSharedPtr<SIRGLSampler> SIRGLProgrammableDevice::createSampler( const SIRSharedPtr<SIRRenderDevice>& owner, const SIRRenderSamplerDesc& description ) noexcept
{
	return sirGLFactoryOwner( owner, *this ) ? SIRMakeShared<SIRGLSampler>( sirGLFactoryCallbacks(), owner, description ) : SIRSharedPtr<SIRGLSampler>();
}

SIRSharedPtr<SIRGLSync> SIRGLProgrammableDevice::createSynchronization( const SIRSharedPtr<SIRRenderDevice>& owner, SIRRenderSynchronizationType type ) noexcept
{
	return sirGLFactoryOwner( owner, *this ) ? SIRMakeShared<SIRGLSync>( sirGLFactoryCallbacks(), owner, type ) : SIRSharedPtr<SIRGLSync>();
}

SIRSharedPtr<SIRGLCommandList> SIRGLProgrammableDevice::createCommandList( const SIRSharedPtr<SIRRenderDevice>& owner ) noexcept
{
	return sirGLFactoryOwner( owner, *this ) ? SIRMakeShared<SIRGLCommandList>( sirGLFactoryCallbacks(), owner ) : SIRSharedPtr<SIRGLCommandList>();
}

SIRGLQueue::SIRGLQueue( SIRGLProgrammableDevice& device ) noexcept
    : SIRRenderQueue( device, SIRRenderQueueType::SIR_RENDER_QUEUE_GRAPHICS )
{
}

SIRGLQueue::~SIRGLQueue() noexcept
{
	static_cast<void>( waitIdle() );
}

SIRRenderResult SIRGLQueue::submit( const SIRRenderSubmission& submission ) noexcept
{
	if ( submission.commands.getSize() == 0 || submission.commands.getData() == nullptr ) return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	if ( submission.waits.getSize() != submission.waitValues.getSize() || submission.signals.getSize() != submission.signalValues.getSize() ) return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	for (size_t i = 0; i < submission.waits.getSize(); ++i) {
		auto *pWait = dynamic_cast<const SIRGLSync *>( submission.waits[i] );
		if ( pWait == nullptr || const_cast<SIRGLSync *>( pWait )->wait( submission.waitValues[i], UINT64_MAX ) != SIRRenderResult::SIR_RENDER_SUCCESS ) return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	}
	for (size_t i = 0; i < submission.commands.getSize(); ++i) {
		auto *pCommand = dynamic_cast<SIRGLCommandList *>( submission.commands[i] );
		if ( pCommand == nullptr || !pCommand->ended() || pCommand->getDevice().get() != &getDevice() ) return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	}
	if ( m_LastFence != nullptr ) static_cast<SIRGLProgrammableDevice&>( getDevice() ).getProcs().glDeleteSync( m_LastFence );
	m_LastFence = static_cast<SIRGLProgrammableDevice&>( getDevice() ).getProcs().glFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 );
	static_cast<SIRGLProgrammableDevice&>( getDevice() ).getProcs().glFlush();
	if ( m_LastFence == nullptr ) return SIRRenderResult::SIR_RENDER_DRIVER_ERROR;
	for (size_t i = 0; i < submission.signals.getSize(); ++i) {
		auto *pSignal = dynamic_cast<SIRGLSync *>( submission.signals[i] );
		if ( pSignal == nullptr || pSignal->signal( submission.signalValues[i] ) != SIRRenderResult::SIR_RENDER_SUCCESS ) return SIRRenderResult::SIR_RENDER_DRIVER_ERROR;
	}
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRRenderResult SIRGLQueue::waitIdle( void ) noexcept
{
	if (!static_cast<SIRGLProgrammableDevice&>(getDevice()).isReady()) return SIRRenderResult::SIR_RENDER_DEVICE_LOST;
	if (!m_LastFence) { static_cast<SIRGLProgrammableDevice&>(getDevice()).getProcs().glFlush(); return SIRRenderResult::SIR_RENDER_SUCCESS; }
	const GLenum status = static_cast<SIRGLProgrammableDevice&>(getDevice()).getProcs().glClientWaitSync(m_LastFence, GL_SYNC_FLUSH_COMMANDS_BIT, GLuint64(-1));
	static_cast<SIRGLProgrammableDevice&>(getDevice()).getProcs().glDeleteSync(m_LastFence); m_LastFence = nullptr;
	return status == GL_ALREADY_SIGNALED || status == GL_CONDITION_SATISFIED ? SIRRenderResult::SIR_RENDER_SUCCESS : SIRRenderResult::SIR_RENDER_DEVICE_LOST;
}

SIRRenderResult SIRGLQueue::collectCompleted( void ) noexcept
{
	if (!m_LastFence) return SIRRenderResult::SIR_RENDER_SUCCESS;
	const GLenum status = static_cast<SIRGLProgrammableDevice&>(getDevice()).getProcs().glClientWaitSync(m_LastFence, 0, 0);
	if (status == GL_ALREADY_SIGNALED || status == GL_CONDITION_SATISFIED) { static_cast<SIRGLProgrammableDevice&>(getDevice()).getProcs().glDeleteSync(m_LastFence); m_LastFence = nullptr; return SIRRenderResult::SIR_RENDER_SUCCESS; }
	return status == GL_TIMEOUT_EXPIRED ? SIRRenderResult::SIR_RENDER_TIMEOUT : SIRRenderResult::SIR_RENDER_DEVICE_LOST;
}

SIRGLMemoryAllocator::SIRGLMemoryAllocator( SIRGLProgrammableDevice& device ) noexcept
    : SIRRenderMemoryAllocator( device )
{
}

SIRRenderResult SIRGLMemoryAllocator::getBufferRequirements( const SIRRenderBufferDesc& description, SIRRenderMemoryRequirements *pRequirements ) const noexcept
{
	if ( pRequirements == nullptr || description.nSizeBytes == 0 || description.nUsageMask == 0 ) return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	*pRequirements = { description.nSizeBytes, 16, 0 };
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRRenderResult SIRGLMemoryAllocator::getTextureRequirements( const SIRRenderTextureDesc& description, SIRRenderMemoryRequirements *pRequirements ) const noexcept
{
	if ( pRequirements == nullptr || description.nWidth == 0 || description.nHeight == 0 || description.nMipLevels == 0 || description.eFormat == SIRRenderFormat::SIR_RENDER_FORMAT_UNKNOWN ) return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	*pRequirements = { static_cast<uint64_t>( description.nWidth ) * description.nHeight * 4, 4, 0 };
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRRenderResult SIRGLMemoryAllocator::getRenderbufferRequirements( const SIRRenderRenderbufferDesc& description, SIRRenderMemoryRequirements *pRequirements ) const noexcept
{
	if ( pRequirements == nullptr || description.nWidth == 0 || description.nHeight == 0 ) return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	*pRequirements = { static_cast<uint64_t>( description.nWidth ) * description.nHeight * 4, 4, 0 };
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}

SIRRenderResult SIRGLMemoryAllocator::getAccelerationStructureRequirements( uint64_t, SIRRenderMemoryRequirements * ) const noexcept
{
	return SIRRenderResult::SIR_RENDER_UNSUPPORTED;
}

SIRRenderResult SIRGLMemoryAllocator::getMemoryBudget( SIRRenderMemoryBudget *pBudget ) const noexcept
{
	if ( pBudget == nullptr ) return SIRRenderResult::SIR_RENDER_INVALID_ARGUMENT;
	*pBudget = {};
	return SIRRenderResult::SIR_RENDER_SUCCESS;
}
