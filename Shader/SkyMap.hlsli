struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

struct VS_OUT_CUBOID
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
    float3 bearing : BEARING;
    uint instance_id : SV_INSTANCEID;
};

struct GS_OUT_CUBOID
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
    float3 bearing : BEARING;
    uint sv_render_target_array_index : SV_RENDERTARGETARRAYINDEX;
};

cbuffer sky_map_constant_buffer : register(b0)
{
    row_major float4x4 inverse_view_projection;
    float skyMapBrightness; // 明度調整
    float skyMapContrast; // コントラスト調整
};
