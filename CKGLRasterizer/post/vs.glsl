#version 330 core
out vec2 texcoords;
void main()
{
    vec4[4] pos = vec4[4](
        vec4(-1.,  1., 0., 1.),
        vec4(-1., -1., 0., 1.),
        vec4( 1.,  1., 0., 1.),
        vec4( 1., -1., 0., 1.)
    );
    vec2[4] tcoords = vec2[4](
        vec2(0., 1.),
        vec2(0., 0.),
        vec2(1., 1.),
        vec2(1., 0.)
    );
    gl_Position = pos[gl_VertexID];
    texcoords = tcoords[gl_VertexID];
}
