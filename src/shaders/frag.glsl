#version 460 core
in vec2 screenPos;
out vec4 FragColor;

const int MAX_STEPS = 20;
const float TOLERANCE = 0.01;
const float MAX_DIST = 6.2830; // half universe

uniform vec4 cpos;
uniform vec4 up;
uniform vec4 right;
uniform vec4 front;
uniform float u_time;

float focal = 1.2;

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


float apcos(float t)
{
    return sqrt(2.0*(1.0 - t));
}


struct Hit 
{
    vec3 col;
    float t;
    vec4 center;
};

Hit sceneSDF(vec4 pos)
{
    Hit hit;
    hit.t = 10.0;

    for (int i = 0; i < particles.length(); i++)
    {
        float approx = 1.0 - dot(pos, particles[i].center);
        float coeff = dot(pos,pos-particles[i].center);
        if (approx > (hit.t + particles[i].radius))
            continue;

        float ang = sqrt(max(0.0, 2.0*approx));

        float ripple = 0.05*sin(100.0*coeff);

        float d = ang + ripple - particles[i].radius;

        if (d < hit.t)
        {
            hit.t = d;
            hit.center = particles[i].center;
            hit.col = particles[i].color;
        }
    }
    return hit;
}


vec4 marchOnSphere(vec4 origin, vec4 dir, float t)
{
    return cos(t)*origin + sin(t)*dir;
}

vec4 normalS3(vec4 p, vec4 c)
{
    vec4 n = c - p*dot(p,c);      
    n = n - p*dot(n,p);           
    return normalize(n);
}


float hash3(vec3 p)
{
    return fract(sin(dot(p, vec3(127.1,311.7,74.7))) * 43758.5453);
}

vec3 sky(vec4 rd)
{
    vec3 d = normalize(rd.xyz);

    float scale = 140.0;
    vec3 cell = floor(d*scale);
    vec3 local = fract(d*scale) - 0.5;

    float rnd = hash3(cell);

    vec3 col = vec3(0.01,0.015,0.03); // base space color

    if(rnd < 0.012)
    {
        vec3 offset = vec3(
            fract(rnd*13.1),
            fract(rnd*7.7),
            fract(rnd*19.3)
        ) - 0.5;

        float dist = length(local - offset);
        float glow = exp(-150.0*dist);

        float brightness = 0.7 + 2.0*fract(rnd*50.0);
        col += vec3(1.0,0.95,0.9)*glow*brightness;
    }

    // faint galaxy band
    float band = exp(-6.0*abs(d.y));
    col += vec3(0.15,0.18,0.25)*band*0.4;

    return col;
}

//////////////////////////////////////////////////////
// MAIN
void main()
{
    // build ray in tangent space
    vec4 rd = normalize(screenPos.x*right + screenPos.y*up + focal*front);
    rd = normalize(rd - cpos*dot(rd,cpos));

    vec4 p = cpos;
    float t = 0.0;

    Hit hit;

    for(int i=0;i<MAX_STEPS;i++)
    {
        hit = sceneSDF(p);

        if(hit.t < TOLERANCE)
        {
            p = marchOnSphere(cpos, rd, t);

            vec4 n = normalS3(p, hit.center);
            vec4 lightDir = normalize(front - cpos*dot(front,cpos));

            float diff = max(dot(n,lightDir),0.0);
            float ambient = 0.18;

            FragColor = vec4(hit.col*(ambient + diff),1.0);
            return;
        }

        t += hit.t;
        if(t > MAX_DIST) break;

        p = marchOnSphere(cpos, rd, t);
    }

    // miss ? sky
    FragColor = vec4(sky(rd),1.0);
}
