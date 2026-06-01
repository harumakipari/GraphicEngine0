#include "SkyMap.hlsli"
#include "FilterFunctions.hlsli"
#include "Sampler.hlsli"

TextureCube skybox : register(t0); //  緯度経度マッピングされたテクスチャ

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 R = mul(float4((pin.texcoord.x * 2.0) - 1.0, 1.0 - (pin.texcoord.y * 2.0), 1.0, 1.0), inverse_view_projection);
    R /= R.w;

    const float lod = 0;
    float4 skyColor = skybox.SampleLevel(samplerStates[LINEAR], R.xyz, lod);

    //skyColor.rgb = BrightnessContrast(skyColor.rgb, skyMapBrightness, skyMapContrast);
    //skyColor.rgb *= 3.0f;

    return skyColor;
}

