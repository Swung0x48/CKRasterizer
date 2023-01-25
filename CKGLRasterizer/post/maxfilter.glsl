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
    vec4 maxc = vec4(0.);
    for (float r = 1.0; r <= radius; r += stp)
    {
        vec2 d[8] = vec2[8](vec2(-1, -1), vec2(-1, 0), vec2(-1, 1),
                            vec2( 0, -1),              vec2( 0, 1),
                            vec2( 1, -1), vec2( 1, 0), vec2( 1, 1));
        for (int i = 0; i < 8; ++i)
        {
            vec4 c = texture(color_in, clamp2(texcoords + r * d[i] * texel_size));
            maxc = max(maxc, c);
        }
    }
    color = maxc;
    norpth = texture(norpth_in, texcoords);
    gl_FragDepth = norpth.w;
}
