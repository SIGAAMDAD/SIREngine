#ifndef __RENDER_FRAMEBUFFER_H__
#define __RENDER_FRAMEBUFFER_H__

#pragma once

#include <Engine/Core/SIREngine.h>
#include <Engine/RenderLib/RenderCommon.h>

#define MAX_FRAMEBUFFER_COLOR_ATTACHMENTS 16

typedef enum {
    FBA_COLORBUFFER,
    FBA_BLOOMBUFFER,
    FBA_DEPTHBUFFER
} FramebufferAttachmentType_t;

typedef enum {
    Framebuffer_Multisample     = 0x1000
} FramebufferFlag_t;

typedef struct {
    const char *pName;
    const FramebufferAttachmentType_t *pAttachments;
    uint64_t nAttachmentCount;
    uint32_t nWidth;
    uint32_t nHeight;
    FramebufferFlag_t nFlags;
} FramebufferInfo_t;

class IRenderFramebuffer
{
public:
    IRenderFramebuffer( void )
        : m_nWidth( 0 ), m_nHeight( 0 )
    { }
    virtual ~IRenderFramebuffer()
    { }

    virtual void Blit( IRenderFramebuffer& dst ) = 0;
    virtual void Draw( void ) const = 0;
protected:
    CString m_Name;
    uint32_t m_nWidth;
    uint32_t m_nHeight;
};

#endif