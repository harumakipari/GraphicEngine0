struct VS_IN
{
    float3 pos : POSITION;
    float alpha : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

struct VS_OUT
{
    float4 pos : SV_POSITION;
    float alpha : TEXCOORD0;
    float2 uv : TEXCOORD1;
};