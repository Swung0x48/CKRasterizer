#version 450

layout (std140, binding=0) uniform MatricesUniformBlock
{
    mat4 world;
    mat4 view;
    mat4 proj;
} m;

layout (location=0) in vec3 xyz;
layout (location=1) in vec3 normal;
layout (location=2) in vec2 texcoord;

layout (location=0) out vec3 fragColor;

void main() {
    gl_Position = m.proj * m.view * m.world * vec4(xyz, 1.0);
    fragColor = vec3(sin(texcoord.x), cos(texcoord.y), sin(texcoord.y));
}
