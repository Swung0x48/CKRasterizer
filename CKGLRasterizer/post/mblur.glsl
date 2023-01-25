#version 330 core
/* BEGIN POST STAGE CONFIGURATION *\
uniform_parameter|attenuation|f32
\*  END POST STAGE CONFIGURATION  */
uniform vec2 screen_size;
uniform vec2 mouse_pos;
uniform float time;
uniform float frame_time;
uniform sampler2D color_in;
uniform sampler2D norpth_in;
uniform sampler2D feedback_in;
uniform float attenuation;
in vec2 texcoords;
layout(location = 0) out vec4 color;
layout(location = 1) out vec4 norpth;
layout(location = 2) out vec4 feedback;
void main()
{
    float range = 24;
    float target_frametime = 1. / 60.;
    float corrected_atten = pow(attenuation, frame_time / target_frametime);
    color = texture(color_in, texcoords);
    norpth = texture(norpth_in, texcoords);
    vec4 fb = texture(feedback_in, texcoords);
    feedback = vec4((fb.xyz * corrected_atten + color.xyz * (1 - corrected_atten)), 1.);
    color = feedback;
    gl_FragDepth = norpth.w;
}
