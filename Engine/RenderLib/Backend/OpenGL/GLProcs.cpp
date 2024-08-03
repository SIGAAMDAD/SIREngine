#include "GLCommon.h"
#include "GLContext.h"

#define NGL( ret, name, ... ) PFN ## name n ## name = NULL;
NGL_Core_Procs
NGL_Debug_Procs
NGL_Buffer_Procs
NGL_FBO_Procs
NGL_Shader_Procs
NGL_GLSL_SPIRV_Procs
NGL_Texture_Procs
NGL_VertexArray_Procs
NGL_BufferARB_Procs
NGL_VertexArrayARB_Procs
NGL_VertexShaderARB_Procs

NGL_ARB_buffer_storage
NGL_ARB_map_buffer_range
NGL_ARB_sync
#undef NGL

void GLContext::InitGLProcs( void )
{
#define NGL( ret, name, ... ) \
    n ## name = ( PFN ## name )SDL_GL_GetProcAddress( #name ); \
    if ( !n ## name ) { SIRENGINE_WARNING( "Couldn't load GL function proc \"" #name "\"" ); }

    NGL_Core_Procs
    NGL_Buffer_Procs
    NGL_FBO_Procs
    NGL_Shader_Procs
    NGL_Texture_Procs
    NGL_VertexArray_Procs
#undef NGL
}
