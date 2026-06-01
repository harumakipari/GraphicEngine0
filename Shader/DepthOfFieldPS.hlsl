#include "Sampler.hlsli"
#include "FullScreenQuad.hlsli"
#include "Constants.hlsli"

Texture2D color_map : register(t0);
Texture2D bokeh_map : register(t1);
Texture2D depth_map : register(t2);


float4 main(VS_OUT pin) : SV_TARGET
{
    //  深度値を取得
    float depth = depth_map.Sample(samplerStates[POINT], pin.texcoord);

    //  深度値から距離を求める
    //  float view_space_z = near * far / (far - depth * (far - near));
    float view_space_z = cameraClipDistance.z / (cameraClipDistance.y - depth * cameraClipDistance.w);

    //  焦点距離と焦点範囲からブレンド係数を算出
    float alpha = abs(view_space_z - focusDistance) / dofRange;
    alpha = saturate(alpha);

    //  ぼかし色取得
    float3 origin_color = color_map.Sample(samplerStates[LINEAR], pin.texcoord).rgb;
    float3 bokeh_color = bokeh_map.Sample(samplerStates[LINEAR], pin.texcoord).rgb;
    float3 color = bokeh_color * (alpha) + origin_color.rgb * (1.0 - alpha);

    return float4(color, 1);
}
