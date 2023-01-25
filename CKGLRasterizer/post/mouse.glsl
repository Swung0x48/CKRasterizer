#version 330 core
uniform vec2 screen_size;
uniform vec2 mouse_pos;
uniform float time;
uniform float frame_time;
uniform sampler2D color_in;
uniform sampler2D norpth_in;
uniform sampler2D feedback_in;
in vec2 texcoords;
layout(location = 0) out vec4 color;
layout(location = 1) out vec4 norpth;
layout(location = 2) out vec4 feedback;
void main()
{
    float range = 24;
    float target_frametime = 1. / 60.;
    float attenuation = 0.9;
    float corrected_atten = pow(attenuation, frame_time / target_frametime);
    color = texture(color_in, texcoords);
    norpth = texture(norpth_in, texcoords);
    vec4 fb = texture(feedback_in, texcoords);
    vec2 mp = vec2(mouse_pos.x, screen_size.y - mouse_pos.y);
    float intensity = pow(clamp(1 - length(mp - (texcoords * screen_size)) / 24, 0., 1.), 3.);
    float combined_intensity = clamp(intensity + fb.r * corrected_atten, 0., 1.);
    vec4 combined = vec4(vec3(1.), combined_intensity);
    feedback = vec4(combined_intensity, 0., 0., 1.);
    color = vec4(color.rgb * (1. - combined.a) + combined.rgb * combined.a, 1.);
    gl_FragDepth = norpth.w;
}
