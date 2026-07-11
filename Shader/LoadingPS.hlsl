#include "Constants.hlsli"

struct VS_OUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};


void mainImage(out float4 fragColor, in float2 fragCoord)
{

    float2 fullscreen = iResolution.xy;
    fullscreen = float2(1920, 1080);

    const float DOTS = 8.0;
    const float3 COLOR = float3(0.3, 0.6, 1.0);

    float2 p = (fragCoord.xy * 2.0 - fullscreen.xy) / min(fullscreen.x, fullscreen.y);

    float f = 0.0;
    
    for (float i = 1.0; i <= DOTS; i++)
    {
        float s = sin(0.7 * elapsedTime + (i * 0.5) * elapsedTime) * 0.2;
        float c = cos(0.2 * elapsedTime + (i * 0.5) * elapsedTime) * 0.2;
        f += 0.01 / abs(length(p * 0.5 + float2(c, s)));
    }
    
    fragColor = float4(COLOR * f, 1.0);
}


float4 main(VS_OUT pin) : SV_Target
{
    float4 fragColor = float4(0, 0, 0, 1);
    float2 fragCoord = pin.position.xy;
    mainImage(fragColor, fragCoord);
    return fragColor;
}