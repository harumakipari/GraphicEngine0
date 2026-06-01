//--------------------------------------------
// リムライト
//--------------------------------------------
// N:法線(正規化済み)
// E:視点方向ベクトル(正規化済み)
// L:入射ベクトル(正規化済み)
// C :ライト色
// RimPower : リムライトの強さ
float3 CalcRimLight(float3 N, float3 V, float3 color, float power)
{
    float rim = 1.0 - saturate(dot(N, V));
    rim = smoothstep(0.0, 1.0, rim);

    //float rim = dot(N, V);
    //rim = smoothstep(1.0, 0.0, rim);

    rim = pow(rim, power);
    return rim * color;
}


//--------------------------------------------
//	フォグ
//--------------------------------------------
//color:現在のピクセル色
//fog_color:フォグの色
//fog_range:フォグの範囲情報
//eye_length:視点からの距離
float4 CalcFog(in float4 color, float4 fogColor, float2 fogRange, float eyeLength)
{
    float fogAlpha = saturate((eyeLength - fogRange.x) / (fogRange.y - fogRange.x));
    return lerp(color, fogColor, fogAlpha);
}
