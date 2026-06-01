#include "sprite.hlsli"

Texture2D colorMap : register(t0);
SamplerState pointSamplerState : register(s0);
SamplerState linerSamplerState : register(s1);
SamplerState anisotropocSamplerState : register(s2);

float4 main(VS_OUT pin) : SV_TARGET
{
    float4 color = colorMap.Sample(anisotropocSamplerState, pin.texCoord);
    float alpha = color.a;
#if 0
    //Glitch
    if (enableGlitch)
    {
        float2 glitchUV = pin.texCoord;
    
        ////横にUVずらし（スキャンライン風）
        //float glitchAmount = sin(deltaTime * 30.0 + pin.texCoord.y * 120.0) * 0.005;
        ////時々大きくブレる
        //float block = step(frac(pin.texCoord.y * 10.0 + deltaTime * 4.0), 0.1);
        //glitchAmount += block * 0.05;
        ////横ブレ
        //glitchUV.y += glitchAmount;
    
        float2 grid = floor(pin.texCoord * 10.0);
        float timeSeed = floor(elapsedTime * 4.0); // 0.25秒ごとに変化
        float rand = frac(sin(dot(grid + timeSeed, float2(12.9898, 78.233))) * 43758.5453);
    
        float t = elapsedTime * 1.0; // 高速変化させたいならスケーリング
        float2 block = floor(pin.texCoord * float2(30 * rand + 1, 10 * rand + 1));
        float2 noise = frac(sin(dot(block, float2(12.9898 + t, 78.233 + t))) * 43758.5433);
        glitchUV.x += (noise.x - 0.5) * 0.1;
    
        //RGBずらし（色収差）
        float2 offset = float2(0.005, 0);
    
        float r = colorMap.Sample(pointSamplerState, glitchUV + offset).r;
        float g = colorMap.Sample(pointSamplerState, glitchUV).g;
        float b = colorMap.Sample(pointSamplerState, glitchUV - offset).b;
        color.rgb = float3(r, g, b);
    
    }
    
    //Inverse gamma process
    const float GAMMA = 2.2;
    color.rgb = pow(color.rgb, GAMMA);
#endif
    color = float4(color.rgb, alpha) * pin.color;
    clip(color.a - 0.01f);
    return color;
    
    
#if 0
    return colorMap.Sample(linerSamplerState, pin.texCoord);
#endif
    
#if 0
    return colorMap.Sample(pointSamplerState, pin.texCoord) * pin.color;
    
#else
    const float2 center = float2(1280 / 2, 720 / 2);
    float distance = length(center - pin.position.xy);
    if (distance > 200)
        return 1;
    else
        return float4(1, 0, 0, 1);
#endif
    
}