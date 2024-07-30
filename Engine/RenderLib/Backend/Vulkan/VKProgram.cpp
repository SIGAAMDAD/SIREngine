#include "VKProgram.h"
#include "VKShader.h"

VKProgram::VKProgram( const RenderProgramInit_t& programInfo )
{
    RenderShaderInit_t shaderInfo;

    shaderInfo.pName = programInfo.pName;
    shaderInfo.nType = ST_VERTEX;
    m_pVertexShader = new VKShader( shaderInfo );

    shaderInfo.pName = programInfo.pName;
    shaderInfo.nType = ST_FRAGMENT;
    m_pFragmentShader = new VKShader( shaderInfo );
}

VKProgram::~VKProgram()
{

}