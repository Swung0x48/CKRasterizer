#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout (location=0) in vec3 fragColor;
layout (location=1) in vec2 texcoord;

layout (location=0) out vec4 outColor;

layout (binding=1) uniform sampler2D textures[];

layout (push_constant) uniform ActiveTexture
{
    int id[8];
} atex;

void main() {
    //outColor = vec4(fragColor, 1.0);
    outColor = texture(textures[atex.id[0]], texcoord);
}
