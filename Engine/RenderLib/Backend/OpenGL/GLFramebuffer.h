#ifndef __GL_FRAMEBUFFER_H__
#define __GL_FRAMEBUFFER_H__

#if defined(SIRENGINE_PRAGMA_ONCE_SUPPORTED)
    #pragma once
#endif

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>
#include "GLCommon.h"
#include "../RenderFramebuffer.h"

namespace SIREngine::RenderLib::Backend::OpenGL {
    typedef struct {
        FramebufferAttachmentType_t nType;
        GLuint hBufferID;
        GLenum nAttachmentID;
    } FramebufferAttachmentData_t;

    class GLFramebuffer : public Backend::IRenderFramebuffer
    {
    public:
        GLFramebuffer( const FramebufferInfo_t& info );
        virtual ~GLFramebuffer() override;

        virtual void Blit( Backend::IRenderFramebuffer& dstFbo ) override;
        virtual void Draw( void ) const override;
    private:
        void AllocateBuffer( const FramebufferAttachmentType_t nType );

        FramebufferAttachmentData_t m_szColorAttachments[ MAX_FRAMEBUFFER_COLOR_ATTACHMENTS ];
        FramebufferAttachmentData_t m_hDepthAttachment;

        GLuint m_hFramebufferID;
        uint32_t m_nColorAttachments;
    };
};

#endif