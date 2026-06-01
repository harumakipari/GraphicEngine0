struct GBUFFER_PS_OUT
{
    float4 albedo : SV_TARGET1;
    float4 material : SV_TARGET2; // x:metallic y:roughness z:occlusion w:materialType
    float4 gBuffer3Normal : SV_TARGET3; // world gBuffer1Normal  w:objectType
    float4 emissive : SV_TARGET4; // w:何かを書き込んでいたら０にするそれ以外は１　スカイマップなどの時に使用 2 emissiveFlagとして使用
    float4 position : SV_TARGET5; // world position
    //float4 velocity : SV_TARGET6;
};

