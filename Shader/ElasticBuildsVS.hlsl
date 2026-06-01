#include "Constants.hlsli"
#include "ElasticConstants.hlsli"
#include "GltfModel.hlsli"


VS_OUT main(VS_IN vin)
{
    VS_OUT vout;
    float sigma = vin.tangent.w;
#if 0

    // ① モデル→ワールド
    float4 worldPos4 = mul(vin.position, world);
    float3 worldPos = worldPos4.xyz;

    // ② t (高さ比)
    float meshHeight = max(buildHeight, 1e-5);

    float3 testDir = normalize(float3(0, 0, 1)); // Z+ 方向
    float testLength = 1.0;

#if 1
    float t = saturate((worldPos.y - p1.y) / meshHeight);
#else
// 原点を基準に投影
    float axisDist = dot(worldPos, testDir);
    float t = saturate(axisDist / testLength);
#endif


    // ③ ベジェ点 & 接線（ワールド空間）
    float3 bezierPos = QuadricBezier(p1.xyz, p2.xyz, p3.xyz, t);
    float3 bezierTangentRaw = QuadricBezierTangent(p1.xyz, p2.xyz, p3.xyz, t);
    float3 bezierTangent = SafeNormalize(bezierTangentRaw, float3(0, 0, 1));

#if 1
    // ④ 直線の芯とローカルオフセット
    float3 straightPos = p1.xyz + float3(0, (worldPos.y - p1.y), 0);
    float3 localOffset = worldPos - straightPos;

#else
    float3 straightPos = base + pullDir * axisDist;
    float3 localOffset = worldPos - straightPos;
#endif

    // ⑤ 回転軸・角度の準備（安定処理）
    float3 up = float3(0, 1, 0);
    float3 rotAxisRaw = cross(up, bezierTangent);
    float axisLen = length(rotAxisRaw);

    // デフォルト：回転しない (axisLen が小さい = up と接線が平行)
    float3 rotatedOffset = localOffset;
    float3 outNormal = vin.normal.xyz;
    float3 outTangent = vin.tangent.xyz;

    if (axisLen > 1e-5)
    {
        // 正規化
        float3 rotAxis = rotAxisRaw / axisLen;
#if 1
        float cosAngle = clamp(dot(up, bezierTangent), -1.0, 1.0);
        float angle = acos(cosAngle);
        float maxAngle = radians(maxAngleDegree); // 30 度以上は回転しない
        angle = clamp(angle, -maxAngle, maxAngle);
        float sinAngle = sin(angle);
        //float sinAngle = sqrt(max(0.0, 1.0 - cosAngle * cosAngle));

#else
        float angle = atan2(axisLen, clamp(dot(up, bezierTangent), -1.0, 1.0));
        float maxAngleDeg = 30.0; // 30 度以上は回転しない
        float maxAngle = maxAngleDeg * 3.14159265 / 180.0;
        
        float useAngle = clamp(angle, -maxAngle, maxAngle);
        float cosAngle = cos(useAngle);
        float sinAngle = sin(useAngle);

#endif
        // Rodriguesでローカルオフセットを回転
        rotatedOffset = RodriguezRotate(localOffset, rotAxis, cosAngle, sinAngle);
        
        // 法線／接線も同じ回転を適用する（まずワールドに変換しておく）
        outNormal = normalize(mul(vin.normal.xyz, (float3x3) world));
        outTangent = normalize(mul(vin.tangent.xyz, (float3x3) world));

        outNormal = RodriguezRotate(outNormal, rotAxis, cosAngle, sinAngle);
        outTangent = RodriguezRotate(outTangent, rotAxis, cosAngle, sinAngle);
    }
    else
    {
        // 回転不要: 法線／接線をワールド変換だけしておく
        outNormal = normalize(mul((float3x3) world, vin.normal.xyz));
        outTangent = normalize(mul((float3x3) world, vin.tangent.xyz));
    }

    // ⑥ 変形頂点
    float3 deformedPos = bezierPos + rotatedOffset;

    // ⑦ 出力
    vout.wPosition = float4(deformedPos, 1.0f);
    vout.position = mul(vout.wPosition, viewProjection);

    vout.wNormal = float4(SafeNormalize(outNormal, float3(0, 1, 0)), 0.0f);
    vout.wTangent = float4(SafeNormalize(outTangent, float3(1, 0, 0)), sigma);

    vout.texcoord = vin.texcoord;
    return vout;

#else

    float3 localPos = vin.position.xyz;
    float meshHeight = max(buildHeight, 1e-5);
    float t = saturate((localPos.y - p1.y) / meshHeight);
    float3 bezierPos = QuadricBezier(p1.xyz, p2.xyz, p3.xyz, t);
    float3 bezierTangent = SafeNormalize(QuadricBezierTangent(p1.xyz, p2.xyz, p3.xyz, t), float3(0, 1, 0));

    float3 straightPos = p1.xyz + float3(0, (localPos.y - p1.y), 0);
    float3 localOffset = localPos - straightPos;

    float3 up = float3(0, 1, 0);
    float3 rotAxisRaw = cross(up, bezierTangent);
    float axisLen = length(rotAxisRaw);

    float3 rotatedOffset = localOffset;
    float3 localNormal = vin.normal.xyz;
    float3 localTangent = vin.tangent.xyz;

    if (axisLen > 1e-5)
    {
        float3 rotAxis = rotAxisRaw / axisLen;
        float cosAngle = clamp(dot(up, bezierTangent), -1.0, 1.0);
        float angle = acos(cosAngle);
        float maxAngle = radians(maxAngleDegree);
        angle = clamp(angle, -maxAngle, maxAngle);

        float s = sin(angle);
        float c = cos(angle);

        rotatedOffset = RodriguezRotate(localOffset, rotAxis, c, s);
        localNormal = RodriguezRotate(localNormal, rotAxis, c, s);
        localTangent = RodriguezRotate(localTangent, rotAxis, c, s);
    }

    float3 deformedLocalPos = bezierPos + rotatedOffset;

    float4 worldPos = mul(float4(deformedLocalPos, 1.0f), world);

    vout.wPosition = worldPos;
    vout.position = mul(worldPos, viewProjection);

    vout.wNormal = float4(
    normalize(mul((float3x3) world, localNormal)), 0.0f);

    vout.wTangent = float4(
    normalize(mul((float3x3) world, localTangent)), sigma);


    vout.texcoord = vin.texcoord;
    return vout;

#endif
}