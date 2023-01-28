#version 330 core
/* BEGIN POST STAGE CONFIGURATION *\
uniform_parameter|kradius|i32
uniform_parameter|kernel|f32[65]
uniform_parameter|convdir|f32v2
\*  END POST STAGE CONFIGURATION  */
//chris: this is slow as f! we need something better.
uniform vec2 screen_size;
uniform vec2 mouse_pos;
uniform float time;
uniform float frame_time;
uniform sampler2D color_in;
uniform sampler2D norpth_in;
uniform int kradius;
uniform float[65] kernel;
uniform vec2 convdir;
in vec2 texcoords;
layout(location = 0) out vec4 color;
layout(location = 1) out vec4 norpth;
void main()
{
    vec2 texel_size = 1. / screen_size;
    float ksum = 0.;
    vec3 csum = vec3(0.);
    for (int i = -kradius; i <= kradius; ++i)
    {
        ksum += kernel[i + kradius];
        csum += kernel[i + kradius] * texture(color_in, texcoords + convdir * float(i) * texel_size).rgb;
    }
    color = vec4(csum / ksum, 1.);
    norpth = texture(norpth_in, texcoords);
    gl_FragDepth = norpth.w;
}
