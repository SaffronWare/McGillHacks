#version 460 core

out vec2 screenPos; // final ray-space coordinates

uniform vec2 u_resolution;

void main()
{
    // fullscreen triangle
    vec2 verts[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2( 3.0, -1.0),
        vec2(-1.0,  3.0)
    );

    vec2 p = verts[gl_VertexID];

    // convert to normalized screen space [-1,1]
    vec2 uv = p;

    // aspect ratio correction
    float aspect = u_resolution.x / u_resolution.y;
    screenPos = vec2(uv.x * aspect, uv.y);

    gl_Position = vec4(p, 0.0, 1.0);
}
