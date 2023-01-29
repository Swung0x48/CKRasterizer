#version 330 core
/* BEGIN POST STAGE CONFIGURATION *\
uniform_parameter|edge_color|f32v4
uniform_parameter|background|f32v4
\*  END POST STAGE CONFIGURATION  */
uniform vec2 screen_size;
uniform vec2 mouse_pos;
uniform float time;
uniform float frame_time;
uniform sampler2D color_in;
uniform sampler2D norpth_in;
uniform vec4 edge_color;
uniform vec4 background;
in vec2 texcoords;
layout(location = 0) out vec4 color;
layout(location = 1) out vec4 norpth;
//smoothstep but no undefined behavior when e0 >= e1
float smoothstp(float e0, float e1, float x)
{
    float t = clamp((x - e0) / (e1 - e0), 0., 1.);
    return t * t * (3. - 2. * t);
}
float noise(float x)
{
    //taken from https://www.shadertoy.com/view/4djSRW
    //by David Hoskins, MIT License
    x = fract(x * .1031);
    x *= x + 33.33;
    x *= x + x;
    return fract(x);
}
void main()
{
    vec2 texel_size = 1. / screen_size;
    norpth = texture(norpth_in, texcoords);
    if (isnan(norpth.x))
        color = vec4(background.rgb, 1.);
    else
    {
        vec4 avg = vec4(0.);
        vec2 d[8] = vec2[8](vec2(-1, -1), vec2(-1, 0), vec2(-1, 1),
                            vec2( 0, -1),              vec2( 0, 1),
                            vec2( 1, -1), vec2( 1, 0), vec2( 1, 1));
        for (int i = 0; i < 8; ++i)
            avg += texture(norpth_in, texcoords + d[i] * texel_size);
        avg /= 8;
        float thresh = 0.01;
        float r = 0.;
        for (int t = 0; t < 5; ++t)
        {
            vec2 p = texcoords * float(t + 1) * screen_size;
            r += noise(dot(p, p) + p.x + p.y);
        }
        r = fract(r);
        float depth_mod = 1. / (1 - pow(norpth.w, 3.6));
        depth_mod = smoothstp(5, 15, depth_mod);
        depth_mod = mix(1., depth_mod, r);
        float edge_atten = smoothstp(1. - 1. / 72., 1. - 1. / 144., norpth.w);
        color = mix(vec4(vec3(depth_mod), 1.), edge_color, step(thresh, length(norpth - avg)));
        color = mix(color, vec4(0.), edge_atten);
        color.a = 1.;
    }
    gl_FragDepth = norpth.w;
}
