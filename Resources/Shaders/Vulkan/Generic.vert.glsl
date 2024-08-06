#version 450

layout( location = 0 ) in vec3 a_Position;
layout( location = 1 ) in vec2 a_TexCoords;
layout( location = 2 ) in vec4 a_Color;
//layout( location = 2 ) in u16vec4 a_Color;

layout( location = 0 ) out vec4 v_Color;
layout( location = 1 ) out vec2 v_TexCoords;

layout( std140, binding = 4 ) uniform u_VertexInput {
    layout( offset = 0 ) mat4 u_ModelViewProjection;
} VertexInput;

void main() {
    gl_Position = vec4( a_Position, 1.0 ) * VertexInput.u_ModelViewProjection;
    v_Color = a_Color;
    v_TexCoords = a_TexCoords;
}