
cbuffer PARTICLE_CONSTANTS : register(b12)
{
    uint particle_count;
    float particle_size;
    float particle_option;
    float delta_time;
    float4 position_on_near_plane;
    float4 eye_position;
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
};