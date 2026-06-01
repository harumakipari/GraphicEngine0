#ifndef __IMAGE_BASED_LIGHTING__
#define __IMAGE_BASED_LIGHTING__ 
#include "Sampler.hlsli"

Texture2D textureMaps[4] : register(t0);

Texture2D skybox : register(t32);
TextureCube diffuseIem : register(t33);
TextureCube specularPmrem : register(t34);
Texture2D lutGgx : register(t35);

float4 sampleLutGgx(float2 brdfSamplePoint)
{
    return lutGgx.Sample(samplerStates[LINEAR], brdfSamplePoint);
}

float4 sampleSkybox(float3 v, float roughness)
{
    const float PI = 3.14159265358979;
    uint width, height, numberOfLevels;
    skybox.GetDimensions(0, width, height, numberOfLevels);
    
    float lod = roughness * float(numberOfLevels - 1);
    
    v = normalize(v);
    // Blinn/Newell Latitude Mapping
    float2 samplePoint;
    samplePoint.x = (atan2(v.z, v.x) + PI) / (PI * 2.0);
    samplePoint.y = 1.0 - ((asin(v.y) + PI * 0.5) / PI);
    return skybox.SampleLevel(samplerStates[LINEAR], samplePoint, lod);
}
float4 sampleDiffuseIem(float3 v)
{
    return diffuseIem.Sample(samplerStates[LINEAR], v);
}

float4 sampleSpecularPmrem(float3 v, float roughness)
{
    //uint width, height, number_of_levels;
    //specularPmrem.GetDimensions(0, width, height, number_of_levels);

    //float lod = roughness * float(number_of_levels - 1);

    //return specularPmrem.SampleLevel(samplerStates[LINEAR], v, lod);

    
    uint width, height, numberOfLevels;
    specularPmrem.GetDimensions(0, width, height, numberOfLevels);
    
    float lod = roughness * float(numberOfLevels - 1);
    
    return specularPmrem.SampleLevel(samplerStates[LINEAR], v, lod);
}


#endif
