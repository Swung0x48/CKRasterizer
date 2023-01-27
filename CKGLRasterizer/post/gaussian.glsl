#version 330 core
/* BEGIN POST STAGE CONFIGURATION *\
uniform_parameter|radius|i32
\*  END POST STAGE CONFIGURATION  */
//chris: this is slow as f! we need something better.
uniform vec2 screen_size;
uniform vec2 mouse_pos;
uniform float time;
uniform float frame_time;
uniform sampler2D color_in;
uniform sampler2D norpth_in;
uniform int radius;
in vec2 texcoords;
layout(location = 0) out vec4 color;
layout(location = 1) out vec4 norpth;
float normaldist(float x, float s)
{ return 0.39894228 * exp(-0.5 * x * x / s / s) / s; }
void main()
{
    vec2 texel_size = 1. / screen_size;
    int sz = radius * 2 + 1;
    float kernel[33];
    if (sz < 33)
    {
        for (int i = 0; i <= radius; ++i)
            kernel[radius - i] = kernel[radius + i] = normaldist(float(i), 9);
        float kf = 0;
        for (int i = 0; i < sz; ++i)
            kf += kernel[i];
        vec3 acc = vec3(0.);
        for (int i = -radius; i <= radius; ++i)
            for (int j = -radius; j <= radius; ++j)
                acc += texture(color_in, texcoords + vec2(float(i), float(j)) * texel_size).rgb * kernel[radius + i] * kernel[radius + j];
        color = vec4(acc / kf / kf, 1.);
        //color = vec4(vec3(kernel[int(texcoords.x * screen_size.x) % sz]), 1.);
    }
    else color = texture(color_in, texcoords);
    norpth = texture(norpth_in, texcoords);
    gl_FragDepth = norpth.w;
}
