#pragma once
#include <DirectXMath.h>

struct FrameConstant
{
    float elapsedTime = 0.0f;
    float deltaTime = 0.0f;
};

struct ShaderConstant
{
    bool enableSsao;
    bool enableCascadedShadowMaps;
    bool enableSsr;
    bool enableFog;

    bool enableBloom;
    bool enableBlur;
    bool directionalLightEnable = true; // ïΩçsåıåπÇÃ on / off
    bool colorizeCascadeLayer = false;

    float shadowColor = 0.2f;
    float shadowDepthBias = 0.0005f;
};


class SceneRenderContext
{
    
};