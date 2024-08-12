#ifndef __VK_PROGRAM_H__
#define __VK_PROGRAM_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>
#include "VKCommon.h"
#include "../RenderProgram.h"

namespace SIREngine::RenderLib::Backend::Vulkan {
    class VKProgram : public Backend::IRenderProgram
    {
    public:
        VKProgram( const RenderProgramInit_t& programInfo );
        virtual ~VKProgram() override;

        virtual bool Load( void ) override { return false; }
    };
};

#endif