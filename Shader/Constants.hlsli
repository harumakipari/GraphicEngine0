#ifndef CONSTANTS_INCLUDE
#define CONSTANTS_INCLUDE

cbuffer SCENE_CONSTANT_BUFFER : register(b1)
{
    float elapsedTime;
    float deltaTime;
    float2 iResolution; // 画面解像度

    float gravity;
}

cbuffer SHADER_CONSTANT_BUFFER : register(b9)
{
    float shadowColor;
    float shadowDepthBias;
    float slopeBias;
    float splitU;

    float hueShift; // 色相調整
    float saturation; // 彩度調整
    float brightness; // 明度調整
    float contrast; // コントラスト調整

    float focusDistance; // 焦点距離
    float dofNearRange;
    float dofRange; // 被写界深度範囲
    float dofBlurStrength;

    float objectIblIntensity; //オブジェクトごとのiblIntensity
    int renderStep;
    int enableToneMapping;
    int enableSSAO;

    int enableCascadedShadowMaps;
    int enableSSR;
    int enableFog;
    int enableBloom;
    
    int enableBlur;
    int enableDof;
    int colorizeCascadedLayer;
    float toneMappingValue ;

    float3 colorMapRGB;
    float pad3 = 0.0f;

}
cbuffer CSM_CONSTANTS : register(b3)
{
    row_major float4x4 cascadedMatrices[4];
    float4 cascadedPlaneDistances;
}
cbuffer FOG_CONSTANTS_BUFFER : register(b8)
{
    float4 fogColor;
    
    float fogDensity;
    float fogHeightFalloff;
    float groundLevel;
    float fogCutoffDistance;
    
    float mieScatteringCoef;
    float timeScale;
    float noiseScale;
    int enableDither;

    float globalFogIntensity;
    int isWindowFog;
    float fogNear; // 距離fogの開始位置
    float fogFar; // 距離fogの終了位置

    float distanceFogHeightFalloff; // 距離フォグの高さ減衰。0で減衰なし。値が大きいほど距離フォグの影響が高い位置が低くなる。
}
cbuffer VOLUMETRIC_CLOUDSCAPES_CONSTANT_BUFFER : register(b5)
{
    float2 windDirection;
    float2 cloudAltitudesMinMax; // highest and lowest altitudes at which clouds are distributed
    
    float windSpeed; // [0.0, 20.0]
    
    float densityScale; // [0.01,0.2]
    float cloudCoverageScale; // [0.1,1.0]
    float rainCloudAbsorptionScale;
    float cloudTypeScale;
    
    float earthRadius;
    float horizonDistanceScale;
    float lowFrequentlyPerlinMorleySamplingScale;
    float highFrequentlyMorleySamplingScale;
    float cloudDensityLongDistanceScale;
    bool enablePowderedSugarEffect;
    
    uint rayMarchingSteps;
    bool autoRayMarchingSteps;
}

cbuffer VIEW_CONSTANTS_BUFFER : register(b4)
{
    row_major float4x4 viewProjection;
    float4 cameraPositon;
    row_major float4x4 view;
    row_major float4x4 projection;
    row_major float4x4 inverseProjection;
    row_major float4x4 inverseViewProjection;
    row_major float4x4 invView;
    float4 cameraClipDistance;
}

#endif