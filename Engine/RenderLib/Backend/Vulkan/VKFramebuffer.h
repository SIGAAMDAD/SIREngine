#ifndef __VK_FRAMEBUFFER_H__
#define __VK_FRAMEBUFFER_H__

#pragma once

#include "../RenderFramebuffer.h"
#include "VKCommon.h"

class VKFramebuffer : public IRenderFramebuffer
{
public:
    VKFramebuffer( const FramebufferInfo_t& info );
    virtual ~VKFramebuffer() override;

    virtual void Blit( IRenderFramebuffer& dst ) override;
    virtual void Draw( void ) const override;
private:
    VkFramebuffer m_hFramebuffer;
    CVector<VkImageView> m_Attachments;
    CVector<VkImage> m_ColorAttachments;
};

#endif