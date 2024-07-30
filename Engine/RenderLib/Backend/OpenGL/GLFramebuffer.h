#ifndef __GL_FRAMEBUFFER_H__
#define __GL_FRAMEBUFFER_H__

#pragma once

#include "../RenderFramebuffer.h"
#include "GLCommon.h"

typedef struct {
    FramebufferAttachmentType_t nType;
    GLuint hBufferID;
    GLenum nAttachmentID;
} FramebufferAttachmentData_t;

class GLFramebuffer : public IRenderFramebuffer
{
public:
    GLFramebuffer( const FramebufferInfo_t& info );
    virtual ~GLFramebuffer() override;

    virtual void Blit( IRenderFramebuffer& dstFbo ) override;
    virtual void Draw( void ) const override;
private:
    void AllocateBuffer( const FramebufferAttachmentType_t nType );

    FramebufferAttachmentData_t m_szColorAttachments[ MAX_FRAMEBUFFER_COLOR_ATTACHMENTS ];
    FramebufferAttachmentData_t m_hDepthAttachment;

    GLuint m_hFramebufferID;
    uint32_t m_nColorAttachments;
};

#endif