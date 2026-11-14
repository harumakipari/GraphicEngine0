
float2 UvToNdc(float2 uv)
{
    float2 ndc;
    ndc.x = 2.0 * uv.x - 1.0;
    ndc.y = 1.0 - 2.0 * uv.y;
    return ndc;
}
float4 NdcToUv(float4 ndc)
{
    float4 uv;
    uv.x = 0.5 + 0.5 * ndc.x;
    uv.y = 0.5 - ndc.y * 0.5;
    uv.z = ndc.z;
    uv.w = ndc.w;
    return uv;
}
float4 ViewToUv(float3 pos, column_major float4x4 projectionTransform)
{
    float4 ndc = mul(float4(pos, 1.0), projectionTransform);
    ndc /= ndc.w;
    return NdcToUv(ndc);
}



