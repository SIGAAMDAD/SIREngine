#version 450

layout( location = 0 ) out vec4 a_Color;
//layout( location = 1 ) out vec4 a_BrightColor;

layout( location = 0 ) in vec4 v_Color;
layout( location = 1 ) in vec2 v_TexCoords;

struct Light {
    vec4 color;
    uvec2 origin;
    float brightness;
    float range;
    float linear;
    float quadratic;
    float constant;
    int type;
};

layout( std140, binding = 3 ) uniform u_LightBuffer {
    Light u_LightData[];
};

layout( binding = 0 ) uniform sampler2D u_DiffuseMap;
layout( binding = 1 ) uniform sampler2D u_NormalMap;
layout( binding = 2 ) uniform sampler2D u_SpecularMap;

void main() {
    a_Color = texture( u_DiffuseMap, v_TexCoords );
}