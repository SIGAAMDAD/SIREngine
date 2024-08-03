#version 450

layout( location = 0 ) out vec4 a_Color;
layout( location = 1 ) out vec4 a_BrightColor;

struct Light {

};

layout( std140, binding = 0 ) uniform u_LightBuffer {
    Light u_LightData[];
};

void main() {

}