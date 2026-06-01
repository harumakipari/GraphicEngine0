#include "GltfModel.hlsli"

float4 main(VS_OUT pin) : SV_TARGET0
{
    return cpuColor; //デバック用で色指定
    const MaterialConstants m = materials[material];
    
    float4 basecolorFactor = m.pbrMetallicRoughness.baseColorFactor;
    return basecolorFactor;
}