#include "RenderSystem.h"
#include "IRenderContext.h"
#include "Vulkan/VulkanContext.h"

using namespace SIREngine::Rendering;

static SRenderConfig s_RenderConfig;
static IRenderContext *s_pContext;

void RenderSystem::Init( const SRenderConfig& config )
{
	s_RenderConfig = config;

	switch ( s_RenderConfig.eApi ) {
		case ERenderAPI::Vulkan:
			s_pContext = new Vulkan::VulkanContext( config );
			break;
	}
}

void RenderSystem::Update( void )
{
	s_pContext->Update();
}

void RenderSystem::Shutdown( void )
{
	delete s_pContext;
}

void RenderSystem::Restart( void )
{
	Shutdown();
	Init( s_RenderConfig );
}