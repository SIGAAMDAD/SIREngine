#include <Core/Compiler.h>
#include <Core/Pch.h>
#include <Rendering/IRenderContext.h>
#include <Rendering/Vulkan/VulkanContext.h>

using namespace SIREngine::Rendering;
using namespace SIREngine::Rendering::Vulkan;

int main( int argc, char **argv )
{
	SRenderConfig renderConfig{};
	renderConfig.DisplayConfig.fAspectRatio = 1.0f;
	renderConfig.DisplayConfig.nWidth = 640;
	renderConfig.DisplayConfig.nHeight = 480;
	renderConfig.DisplayConfig.pszWindowName = "SIREngine Vulkan Testing";

	IRenderContext *pContext = new VulkanContext( renderConfig );

	SDL_Event event;

	while ( true ) {
		while ( SDL_PollEvent( &event ) ) {
			if ( event.type == SDL_EVENT_QUIT ) {
				return 1;
			}
		}
		pContext->Update();
	}

	delete pContext;

	return 0;
}