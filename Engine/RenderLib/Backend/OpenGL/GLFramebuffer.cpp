#include "GLFramebuffer.h"

extern CVar<uint32_t> render_AntiAliasingType;

GLFramebuffer::GLFramebuffer( const FramebufferInfo_t& info )
{
    uint64_t i;
    const FramebufferAttachmentType_t *pAttachment;

    memset( m_szColorAttachments, 0, sizeof( m_szColorAttachments ) );
    memset( &m_hDepthAttachment, 0, sizeof( m_hDepthAttachment ) );

    nglGenFramebuffers( 1, &m_hFramebufferID );

    nglBindFramebuffer( GL_FRAMEBUFFER, m_hFramebufferID );

    m_nColorAttachments = 0;
    for ( i = 0; i < info.nAttachmentCount; i++ ) {
        AllocateBuffer( info.pAttachments[i] );
    }
    nglBindFramebuffer( GL_FRAMEBUFFER, 0 );
}

GLFramebuffer::~GLFramebuffer()
{
    if ( m_hFramebufferID ) {
        nglDeleteFramebuffers( 1, &m_hFramebufferID );
    }
    for ( const auto& it : m_szColorAttachments ) {
        if ( it.hBufferID ) {
            nglDeleteRenderbuffers( 1, &it.hBufferID );
        }
    }
}

void GLFramebuffer::Blit( IRenderFramebuffer& dstFbo )
{
    GLFramebuffer *pOtherFBO;

    pOtherFBO = dynamic_cast<GLFramebuffer *>( eastl::addressof( dstFbo ) );

    nglBindFramebuffer( GL_READ_FRAMEBUFFER, m_hFramebufferID );
    nglBindFramebuffer( GL_DRAW_FRAMEBUFFER, pOtherFBO->m_hFramebufferID );
    nglBlitFramebuffer( 0, 0, m_nWidth, m_nHeight,
                        0, 0, m_nWidth, m_nHeight,
                        GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
                        GL_NEAREST );
    nglBindFramebuffer( GL_FRAMEBUFFER, 0 );
}

void GLFramebuffer::Draw( void ) const
{
    nglBindFramebuffer( GL_READ_FRAMEBUFFER, m_hFramebufferID );
    nglBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );
    nglBlitFramebuffer( 0, 0, m_nWidth, m_nHeight,
                        0, 0, m_nWidth, m_nHeight,
                        GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
                        GL_NEAREST );
    nglBindFramebuffer( GL_FRAMEBUFFER, 0 );
}

void GLFramebuffer::AllocateBuffer( FramebufferAttachmentType_t nType )
{
    FramebufferAttachmentData_t *pAttachment;

    switch ( nType ) {
    case FBA_COLORBUFFER:
    case FBA_BLOOMBUFFER:
        pAttachment = &m_szColorAttachments[ m_nColorAttachments ];
        pAttachment->nAttachmentID = GL_COLOR_ATTACHMENT0 + m_nColorAttachments;
        m_nColorAttachments++;
        break;
    case FBA_DEPTHBUFFER:
        pAttachment = &m_hDepthAttachment;
        pAttachment->nAttachmentID = GL_DEPTH_STENCIL_ATTACHMENT;
        break;
    };

    nglGenRenderbuffers( 1, &pAttachment->hBufferID );

    nglBindRenderbuffer( GL_RENDERBUFFER, pAttachment->hBufferID );

    if ( render_AntiAliasingType.GetValue() >= AntiAlias_2xMSAA
        && render_AntiAliasingType.GetValue() <= AntiAlias_32xMSAA )
    {
        GLsizei samples;
        GLenum format;

        switch ( render_AntiAliasingType.GetValue() ) {
        case AntiAlias_2xMSAA:
            samples = 2;
            break;
        case AntiAlias_4xMSAA:
            samples = 4;
            break;
        case AntiAlias_8xMSAA:
            samples = 8;
            break;
        case AntiAlias_16xMSAA:
            samples = 16;
            break;
        case AntiAlias_32xMSAA:
            samples = 32;
            break;
        };
        
        switch ( nType ) {
        case FBA_BLOOMBUFFER:
            break;
        case FBA_COLORBUFFER:
            format = GL_RGBA8;
            break;
        };

        nglRenderbufferStorageMultisample( GL_RENDERBUFFER, samples, format, m_nWidth, m_nHeight );
    } else {
        GLenum format;

        switch ( nType ) {
        case FBA_COLORBUFFER:
            format = GL_RGBA8;
            break;
        case FBA_DEPTHBUFFER:
            format = GL_DEPTH24_STENCIL8;
            break;
        };
        
        nglRenderbufferStorage( GL_RENDERBUFFER, format, m_nWidth, m_nHeight );
    }

    nglFramebufferRenderbuffer( GL_FRAMEBUFFER, pAttachment->nAttachmentID, GL_RENDERBUFFER,
        pAttachment->hBufferID );

    nglBindRenderbuffer( GL_RENDERBUFFER, 0 );
}