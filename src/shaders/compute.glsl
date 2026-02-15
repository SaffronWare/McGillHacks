#version 430 core

layout(local_size_x = 3) in;

struct Sphere
{
    vec4 center;
    vec3 color;
    float radius;
    vec4 vel;
};

layout(std430, binding = 0) buffer SphereBuffer
{
    Sphere spheres[];
};

uniform float dt;
const float G = 5000.5; // tbd

const float PI = 3.14159265359;

void main()
{
    uint id = gl_GlobalInvocationID.x;

    if (id >= spheres.length())
        return;

    vec4 p = spheres[id].center;
    vec4 v = spheres[id].vel;

    vec4 acceleration = vec4(0.0);

    for (uint j = 0; j < spheres.length(); j++)
    {
        if (j == id) continue;

        vec4 q = spheres[j].center;

        float dotpq = clamp(dot(p, q), -1.0, 1.0);
        float r = acos(dotpq);

        if (r < 0.001) continue;

        float sinr = sqrt(max(1.0 - dotpq * dotpq, 0.000001));

        // tangent direction along geodesic
        vec4 dir = (q - dotpq * p) / sinr;

        // mass of sphere j
        float radius_j = spheres[j].radius;
        float mass_j = (4.0 / 3.0) * PI * radius_j * radius_j * radius_j;

        // inverse-square law
        float forceMag = G * mass_j / (r * r);

        acceleration += forceMag * dir;
    }

    // velocity update
    v += acceleration * dt;

    // project velocity to tangent space of S^3
    v -= p * dot(p, v);

    // exact geodesic step
    vec4 new_p = p * cos(dt) + v * sin(dt);
    vec4 new_v = v * cos(dt) - p * sin(dt);

    new_p = normalize(new_p);

    spheres[id].center = new_p;
    spheres[id].vel = new_v;
}
