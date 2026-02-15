#version 460 core
in vec2 screenPos;
out vec4 FragColor;
const int MAX_STEPS = 20;
const float TOLERANCE = 0.01;
uniform vec4 cpos;
uniform vec4 up;
uniform vec4 right;
uniform vec4 front;

struct Sphere
{
    vec4 center;
    vec3 color;
    float radius;
    vec4 vel;
};
layout(std430, binding = 0) buffer ParticleBuffer {
    Sphere particles[];
};
float focal = 2;
uniform float u_time;
float apcos(float t)
{
    return sqrt(2*(1-t));
}
float particlesDF(vec4 pos, Sphere s) 
{   
   
    return apcos(clamp(dot(pos,s.center),-1.0,1.0)) - s.radius;
};
struct Hit 
{
    vec3 col;
    float t;
    vec4 center;
};

Hit minSDF2(vec4 pos)
{
    Hit hit;
    hit.t = 10.0;
    for (int i = 0; i < particles.length(); i++)
    {
        float approx = 1.0 - dot(pos, particles[i].center);
        // quick reject: definitely far
        if (approx > (hit.t + particles[i].radius))
            continue;
        float d = sqrt(max(0.0, 2.0*approx)) - particles[i].radius;
        if (d < hit.t)
        {
            hit.t = d;
            hit.center = particles[i].center;
            hit.col = particles[i].color;
        }
    }
    
    return hit;
}

Hit minSDF(vec4 pos)
{
    
    float current_min = 10;
    Hit hit = Hit(vec3(0),10,vec4(0));
    for (int i = 0; i < particles.length(); i++)
    {
        
        float d = particlesDF(pos, particles[i]);
        if (d < hit.t)
        {   
            hit.t = d;
            hit.center = particles[i].center;
            hit.col = particles[i].color;
        }
    }
    return hit;
}
vec4 compute_ray(vec4 origin, vec4 direction, float t)
{
    return cos(t) * origin + sin(t) * direction;
}
void main()
{
    // pretend camera looking along +Z
    vec4 rd = normalize(
      screenPos.x * right
    + screenPos.y * up
    + focal * front
    );
    // project to tangent space
    rd = normalize(rd - cpos * dot(rd, cpos));
    vec4 p = cpos;
    float t=0;
    Hit hit;
    FragColor = vec4(0);
    for (int i =0; i<MAX_STEPS; i++)
    {
        hit = minSDF2(p);
        if (hit.t < TOLERANCE)
        {
            p = compute_ray(cpos,rd,t);
            float s = (1+dot(normalize((p-hit.center)),vec4(p)))/2;
            FragColor = s*vec4(hit.col,1);
            break;
        }
        t += 1 * hit.t;
        p = compute_ray(cpos, rd, t);
    };
    //return FragColor;
  
}
