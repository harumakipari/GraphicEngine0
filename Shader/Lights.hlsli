// ì_åıåπ
struct PointLights
{
    float4 position;
    float4 color;
    float range;
    int attenuationType;
    float2 paddings;
};

cbuffer LIGHT_CONSTANT_BUFFER : register(b11)
{
    float4 lightDirection; // w:attenuation Rate
    float4 colorLight; //w colorPower
    float iblIntensity;
    int directionalLightEnable; // ïΩçsåıåπÇÃ on / off
    int pointLightEnable;
    int pointLightCount;

    float3 rimColor;
    float rimIntensity;

    float rimPower;
    float kca; // attenuationConstant
    float kla; // attenuationLinear
    float kqa; // attenuationQuadratic

    float diffuseIntensity;
    float specularIntensity;
    float pointLightDiffuseIntensity;
    float pointLightSpecularIntensity;
    
    float3 playerRimColor;
    float playerRimIntensity;

    float3 playerHairRimColor;
    float playerHairRimIntensity;

    PointLights pointLights[120];
};


struct SpotLights
{
    float4 position;
    float4 direction;
    float4 color;
    float range;
    float innerCorn;
    float outerCorn;
};

struct AttenuationPreset
{
    float distance;
    float kc;
    float kl;
    float kq;
};

static const AttenuationPreset attenuationPresets[] =
{
    { 7, 1.0f, 0.7f, 1.8f },
    { 13, 1.0f, 0.35f, 0.44f },
    { 20, 1.0f, 0.22f, 0.2f },
    { 32, 1.0f, 0.14f, 0.07f },
    { 50, 1.0f, 0.09f, 0.032f },
    { 65, 1.0f, 0.07f, 0.017f },
    { 100, 1.0f, 0.045f, 0.0075f },
    { 160, 1.0f, 0.027f, 0.0028f },
    { 200, 1.0f, 0.022f, 0.0019f },
    { 325, 1.0f, 0.014f, 0.0007f },
    { 600, 1.0f, 0.007f, 0.0002f },
    { 3250, 1.0f, 0.0014f, 0.000007f },
};
