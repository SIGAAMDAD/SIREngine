#ifndef __SIR_GL_PROGRAMMABLE_DEVICE_H__
#define __SIR_GL_PROGRAMMABLE_DEVICE_H__

#include <Renderer/RHI/SIRRenderDevice.h>
#include <Renderer/RHI/SIRRenderTypes.h>
#include <Core/Containers/SIRSpan.h>
#include <Renderer/Drivers/OpenGL/SIRGLContext.h>
#include <Renderer/Drivers/OpenGL/SIRGLFunctionLoader.h>
#include <Renderer/Drivers/OpenGL/SIRGLQueue.h>
#include <Renderer/Drivers/OpenGL/SIRGLMemoryAllocator.h>

#include <memory>

class SIRGLSceneFixtureRenderer;
class SIRGLStorageBuffer;
class SIRGLTexture;
class SIRGLShader;
class SIRGLProgram;
class SIRGLBindingLayout;
class SIRGLBindingSet;
class SIRGLPipeline;
class SIRGLFramebuffer;
class SIRGLSampler;
class SIRGLSync;
class SIRGLCommandList;
class SIRRenderBindingLayout;

class SIRGLProgrammableDevice final : public SIRRenderDevice
{
public:
	struct PresentWindow
	{
		sirGLPresentWindow_t native{};
		bool active{};
	};
	SIRGLProgrammableDevice( void ) noexcept;
	~SIRGLProgrammableDevice( void ) noexcept override;
	SIRRenderResult initialize( void ) noexcept override;
	SIRRenderResult initializeForWindow( const SIRRenderNativeSurface& ) noexcept;
	SIRRenderQueue *getGraphicsQueue( void ) noexcept override;
	SIRRenderQueue *getComputeQueue( void ) noexcept override;
	SIRRenderQueue *getTransferQueue( void ) noexcept override;
	SIRRenderMemoryAllocator& getMemoryAllocator( void ) noexcept override;
	SIRRenderResult waitIdle( void ) noexcept override;
	SIRRenderResult shutdown( void ) noexcept override;
	bool isLost( void ) const noexcept override;
	SIRRenderResult createPresentWindow( const SIRRenderNativeSurface&, const SIRRenderSwapchainDesc&, PresentWindow * ) noexcept;
	SIRRenderResult resizePresentWindow( PresentWindow *, uint32_t, uint32_t ) noexcept;
	SIRRenderResult beginPresentWindow( PresentWindow * ) noexcept;
	SIRRenderResult swapPresentWindow( PresentWindow * ) noexcept;
	SIRRenderResult destroyPresentWindow( PresentWindow * ) noexcept;
	SIRSharedPtr<SIRGLStorageBuffer> createStorageBuffer( const SIRSharedPtr<SIRRenderDevice>&, const SIRRenderBufferDesc& ) noexcept;
	SIRSharedPtr<SIRGLTexture> createTexture( const SIRSharedPtr<SIRRenderDevice>&, const SIRRenderTextureDesc& ) noexcept;
	SIRSharedPtr<SIRGLShader> createShader( const SIRSharedPtr<SIRRenderDevice>&, const SIRRenderShaderCode& ) noexcept;
	SIRSharedPtr<SIRGLProgram> createProgram( const SIRSharedPtr<SIRRenderDevice>&, SIRSpan<const SIRSharedPtr<SIRGLShader>> ) noexcept;
	SIRSharedPtr<SIRGLBindingLayout> createBindingLayout( const SIRSharedPtr<SIRRenderDevice>&, SIRSpan<const SIRRenderBindingDesc> ) noexcept;
	SIRSharedPtr<SIRGLBindingSet> createBindingSet( const SIRSharedPtr<SIRRenderDevice>&, const SIRSharedPtr<SIRRenderBindingLayout>&, SIRSpan<const SIRRenderBindingDesc> ) noexcept;
	SIRSharedPtr<SIRGLPipeline> createPipeline( const SIRSharedPtr<SIRRenderDevice>&, const SIRSharedPtr<SIRGLBindingLayout>&, const SIRSharedPtr<SIRGLProgram>&, const SIRRenderPipelineDesc& ) noexcept;
	SIRSharedPtr<SIRGLFramebuffer> createFramebuffer( const SIRSharedPtr<SIRRenderDevice>&, const SIRRenderFramebufferDesc&, const SIRSharedPtr<SIRGLTexture>&, const SIRSharedPtr<SIRGLTexture>& ) noexcept;
	SIRSharedPtr<SIRGLSampler> createSampler( const SIRSharedPtr<SIRRenderDevice>&, const SIRRenderSamplerDesc& ) noexcept;
	SIRSharedPtr<SIRGLSync> createSynchronization( const SIRSharedPtr<SIRRenderDevice>&, SIRRenderSynchronizationType ) noexcept;
	SIRSharedPtr<SIRGLCommandList> createCommandList( const SIRSharedPtr<SIRRenderDevice>& ) noexcept;
	SIRRenderResult renderEditorScene( uint32_t, uint32_t, float, float ) noexcept;
	GLuint getEditorSceneTexture( void ) const noexcept;
	const sirGLProcs_t& getProcs( void ) const noexcept { return m_Procs; }
	const sirGLProbe_t& getProbe( void ) const noexcept { return m_Probe; }
	bool isReady( void ) const noexcept { return m_pContext != nullptr && !m_bLost; }

private:
	sirGLNativeContext_t *m_pContext{};
	sirGLProcs_t m_Procs{};
	sirGLProbe_t m_Probe{};
	bool m_bLost{};
	SIRGLQueue m_Queue;
	SIRGLMemoryAllocator m_Allocator;
	std::unique_ptr<SIRGLSceneFixtureRenderer> m_pEditorScene;
};

#endif
