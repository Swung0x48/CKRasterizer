#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout (location=0) in vec2 texcoord[8];
flat layout (location=8) in int atexid[8];

layout (location=0) out vec4 outColor;

layout (binding=1) uniform sampler2D textures[];


void main() {
    if (atexid[0] == -1)
        outColor = vec4(1.);
    else
        outColor = texture(textures[atexid[0]], texcoord[0]);
}
