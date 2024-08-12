#include "RenderShaderPipeline.h"

namespace SIREngine::RenderLib::Backend {

const VertexAttribInfo_t szDefaultVertexAttribs[] = {
    { "a_Position", sizeof( Poly_t ), SIREngine_offsetof( Poly_t, position ), Attrib_Position, VTA_VEC3, true, false },
    { "a_TexCoords", sizeof( Poly_t ), SIREngine_offsetof( Poly_t, texCoords ), Attrib_TexCoords, VTA_VEC2, true, false },
    { "a_Color", sizeof( Poly_t ), SIREngine_offsetof( Poly_t, szColor ), Attrib_Color, VTA_COLOR16, true, true },
    { "a_WorldPos", sizeof( Poly_t ), SIREngine_offsetof( Poly_t, worldPos ), Attrib_WorldPos, VTA_VEC3, true, false },
    { "a_Normal", sizeof( Poly_t ), SIREngine_offsetof( Poly_t, szNormal ), Attrib_Normal, VTA_COLOR16, true, true },
};

const UniformInfo_t szDefaultUniforms[ NumUniforms ] = {
    { "u_DiffuseMap", Uniform_DiffuseMap, UniformType_Sampler, ST_FRAGMENT, sizeof( uint32_t ), 1 },
    { "u_NormalMap", Uniform_NormalMap, UniformType_Sampler, ST_FRAGMENT, sizeof( uint32_t ), 1 },
    { "u_SpecularMap", Uniform_SpecularMap, UniformType_Sampler, ST_FRAGMENT, sizeof( uint32_t ), 1 },
    { "u_LightBuffer", Uniform_LightBuffer, UniformType_DynamicBuffer, ST_FRAGMENT, sizeof( LightBuffer_Uniform_t ), 1 },
    { "u_VertexInput", Uniform_VertexInput, UniformType_StaticBuffer, ST_VERTEX, sizeof( VertexInput_Uniform_t ), 1 },
};

};