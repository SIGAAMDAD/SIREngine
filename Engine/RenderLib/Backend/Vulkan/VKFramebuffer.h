#ifndef __VK_FRAMEBUFFER_H__
#define __VK_FRAMEBUFFER_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>
#include "VKCommon.h"
#include "../RenderFramebuffer.h"

namespace SIREngine::RenderLib::Backend::Vulkan {
    class VKFramebuffer : public IRenderFramebuffer
    {
    public:
        VKFramebuffer( const FramebufferInfo_t& info );
        virtual ~VKFramebuffer() override;

        virtual void Blit( IRenderFramebuffer& dst ) override { }
        virtual void Draw( void ) const override { }
    private:
        VkFramebuffer m_hFramebuffer;
        CVector<VkImageView> m_Attachments;
        CVector<VkImage> m_ColorAttachments;
    };
};

#endif