#version 430 core

// how many GPU threads per group
layout(local_size_x = 1) in;


// ----- same struct as fragment shader -----
struct Sphere
{
    vec4 center;
    vec3 color;
    float radius;
    vec4 vel;
};

// binding = 0 must match C++ glBindBufferBase
layout(std430, binding = 0) buffer SphereBuffer
{
    Sphere spheres[];
};


// small timestep
uniform float dt;


void main()
{
    //float dt =0;
    uint id = gl_GlobalInvocationID.x;

    // safety: ignore extra threads
    if (id >= spheres.length())
        return;


    // ----- read -----
    vec4 p = spheres[id].center;
    vec4 v = spheres[id].vel;


    // geodesic update
    vec4 new_p = p*cos(dt) + v*sin(dt);
    vec4 new_v = v*cos(dt) - p*sin(dt);

    new_p = normalize(new_p);


    // ----- write back -----
    spheres[id].center = (new_p.xyzw);
    spheres[id].vel = new_v;
}