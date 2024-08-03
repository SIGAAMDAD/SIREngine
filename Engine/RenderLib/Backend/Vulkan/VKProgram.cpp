#include "VKProgram.h"
#include "VKShader.h"
#include "VKContext.h"

VKProgram::VKProgram( const RenderProgramInit_t& programInfo )
{
    RenderShaderInit_t shaderInfo;

    shaderInfo.pName = programInfo.pName;
    shaderInfo.nType = ST_VERTEX;
    m_pVertexShader = IRenderShader::Create( shaderInfo );

    shaderInfo.pName = programInfo.pName;
    shaderInfo.nType = ST_FRAGMENT;
    m_pFragmentShader = IRenderShader::Create( shaderInfo );
}

VKProgram::~VKProgram()
{
    if ( m_pVertexShader ) {
        dynamic_cast<VKShader *>( m_pVertexShader )->~VKShader();
        g_pVKContext->Free( m_pVertexShader );
    }
    if ( m_pFragmentShader ) {
        dynamic_cast<VKShader *>( m_pFragmentShader )->~VKShader();
        g_pVKContext->Free( m_pFragmentShader );
    }
}