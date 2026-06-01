#include "GltfModel.hlsli"

cbuffer GAME_SCENE_CONSTANT_BUFFER : register(b12)
{
    float2 playerScreenPosition; //プレイヤーの場所　死亡演出に必要な定数バッファ
    float2 screenSize;

    float radius = 0.0f;
    float3 gameOverColor;


    float4 gameMorphWeights;
};




VS_OUT main(MORPH_VS_IN vin)
{
    float sigma = vin.tangent.w;

    float3 morphPos = 0;
    float3 morphNor = 0;

    float m_weights[4]=
    {
        gameMorphWeights.x, gameMorphWeights.y, gameMorphWeights.z, gameMorphWeights.w
    };
    //float m_weights[4]=
    //{
    //    morphWeights.x,morphWeights.y,morphWeights.z,morphWeights.w
    //};

    for (int i = 0; i < 4; i++)
    {
        morphPos += vin.morphPosition[i].xyz * m_weights[i];
        morphNor += vin.morphNormal[i].xyz * m_weights[i];
    }

    vin.position.xyz += morphPos;
    vin.normal.xyz += morphNor;


    // 法線は必ず正規化
    vin.normal.xyz = normalize(vin.normal.xyz);

    if (skin > -1)
    {
        row_major float4x4 skinMatrix =
        jointMatrices[vin.joints[0].x] * vin.weights[0].x +
        jointMatrices[vin.joints[0].y] * vin.weights[0].y +
        jointMatrices[vin.joints[0].z] * vin.weights[0].z +
        jointMatrices[vin.joints[0].w] * vin.weights[0].w +
        jointMatrices[vin.joints[1].x] * vin.weights[1].x +
        jointMatrices[vin.joints[1].y] * vin.weights[1].y +
        jointMatrices[vin.joints[1].z] * vin.weights[1].z +
        jointMatrices[vin.joints[1].w] * vin.weights[1].w;
        vin.position = mul(float4(vin.position.xyz, 1), skinMatrix);
        vin.normal = normalize(mul(float4(vin.normal.xyz, 0), skinMatrix));
        vin.tangent = normalize(mul(float4(vin.tangent.xyz, 0), skinMatrix));
    }
    
    VS_OUT vout;
    
    vin.position.w = 1;
    vout.position = mul(vin.position, mul(world, viewProjection));
    
    //vout.position /= vout.position.w; // to ndc
    
    
    vout.wPosition = mul(vin.position, world);
    
    vin.normal.w = 0;
    vout.wNormal.xyz = normalize(mul(vin.normal, inverseTransposeWorld).xyz);
    vout.wNormal.w = 0;
    
    vin.tangent.w = 0;
    vout.wTangent.xyz = normalize(mul(vin.tangent, inverseTransposeWorld).xyz);
    vout.wTangent.w = sigma;
    
    vout.texcoord = vin.texcoord;
    
    return vout;
}