#version 330 core
/* BEGIN POST STAGE CONFIGURATION *\
uniform_parameter|radius|f32
\*  END POST STAGE CONFIGURATION  */
uniform vec2 screen_size;
uniform vec2 mouse_pos;
uniform float time;
uniform float frame_time;
uniform sampler2D color_in;
uniform sampler2D norpth_in;
uniform float radius;
in vec2 texcoords;
layout(location = 0) out vec4 color;
layout(location = 1) out vec4 norpth;
vec2 clamp2(vec2 x) { return vec2(clamp(x.x, 0., 1.), clamp(x.y, 0., 1.)); }
void main()
{
    color = texture(color_in, texcoords);
    vec2 texel_size = 1. / screen_size;
    float stp = 1.;
    for (float x = 0; x <= radius; x += stp)
        for (float y = 0; y <= radius; y += stp)
        {
            vec2 d[4] = vec2[4](vec2(-x, -y), vec2(-x, y),
                                vec2( x, -y), vec2( x, y));
            float w = step(sqrt(x * x + y * y) / radius, 1.);
            vec4 cc[4];
            for (int i = 0; i < 4; ++i)
                cc[i] = texture(color_in, clamp2(texcoords + d[i] * texel_size));
            vec4 c1 = max(cc[0], cc[1]);
            vec4 c2 = max(cc[2], cc[3]);
            color = mix(color, max(color, max(c1, c2)), w);
        }
    norpth = texture(norpth_in, texcoords);
    gl_FragDepth = norpth.w;
}
