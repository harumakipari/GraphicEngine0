
cbuffer PARTICLE_CONSTANTS : register(b12)
{
    uint particle_count;
    float particle_size;
    float particle_option;
    float delta_time;
    float height_min;
    float height_range;
    float death_progress;
    float detach_speed;
    float gravity_;
    float lifetime;
};

#include "ViewConstants.hlsli"

struct VS_OUT
{
    uint vertex_id : VERTEXID;
};
struct GS_OUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 texcoord : TEXCOORD;
};

struct particle
{
    float4 color;
    float3 position;
    float3 normal;
    float3 velocity;
    float age;
    int state;
    float normalizedHeight;
};
