#version 450

layout( location = 0 ) in vec3 a_Position;
layout( location = 1 ) in vec2 a_TexCoords;
layout( location = 2 ) in vec4 a_Color;

layout( location = 0 ) out vec4 a_OutColor;
layout( location = 1 ) out vec4 a_BrightColor;

layout( std140, binding = 3 ) uniform u_VertexInput {
    mat4 u_ModelViewProjection;
};

layout( binding = 0 ) uniform sampler2D u_DiffuseMap;
layout( binding = 1 ) uniform sampler2D u_NormalMap;
layout( binding = 2 ) uniform sampler2D u_SpecularMap;

void main() {
    gl_Position = vec4( a_Position, 1.0 ) * u_ModelViewProjection;
    a_OutColor = texture( u_DiffuseMap, a_TexCoords );
}