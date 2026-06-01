#ifndef __BIDIRECTIONAL_REFLECTANCE_DISTRIBUTION_FUNCTION_HLSL__ 
#define __BIDIRECTIONAL_REFLECTANCE_DISTRIBUTION_FUNCTION_HLSL__ 

#include "imageBasedLighting.hlsli"
float3 FSchlick(float3 f0, float3 f90, float VoH)
{
    return f0 + (f90 - f0) * pow(clamp(1.0 - VoH, 0.0, 1.0), 5.0);
}

float VGgx(float NoL, float NoV, float alphaRoughness)
{
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;
    
    float ggxv = NoL * sqrt(NoV * NoV * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    float ggxl = NoV * sqrt(NoL * NoL * (1.0 - alphaRoughnessSq) + alphaRoughnessSq);
    
    float ggx = ggxv + ggxl;
    return (ggx > 0.0) ? 0.5 / ggx : 0.0;
}

float DGgx(float NoH, float alphaRoughness)
{
    const float PI = 3.14159265358979;
    float alphaRoughnessSq = alphaRoughness * alphaRoughness;
    float f = (NoH * NoH) * (alphaRoughnessSq - 1.0) + 1.0;
    return alphaRoughnessSq / (PI * f * f);
}

float3 BrdfLambertian(float3 f0, float3 f90, float3 diffuseColor, float VoH)
{
    const float PI = 3.14159265358979;
    return (1.0 - FSchlick(f0, f90, VoH)) * (diffuseColor / PI);
}

float3 BrdfSpecularGgx(float3 f0, float3 f90, float alphaRoughness, float VoH, float NoL, float NoV, float NoH)
{
    float3 F = FSchlick(f0, f90, VoH);
    float Vis = VGgx(NoL, NoV, alphaRoughness);
    float D = DGgx(NoH, alphaRoughness);
    
    return F * Vis * D;
}

float3 IblRadianceLambertian(float3 N, float3 V, float roughness, float3 diffuseColor, float3 f0)
{
    float NoV = clamp(dot(N, V), 0.0, 1.0);
    
    float2 brdfSamplePoint = clamp(float2(NoV, roughness), 0.0, 1.0);
    float2 fAb = sampleLutGgx(brdfSamplePoint).rg;
    
    float3 irradiance = sampleDiffuseIem(N).rgb;
    
    float3 fr = max(1.0 - roughness, f0) - f0;
    float3 k_s = f0 + fr * pow(1.0 - NoV, 5.0);
    float3 fss_ess = k_s * fAb.x + fAb.y;
    
    float ems = (1.0 - (fAb.x + fAb.y));
    float3 f_avg = (f0 + (1.0 - f0) / 21.0);
    float3 fms_ems = ems * fss_ess * f_avg / (1.0 - f_avg * ems);
    
    float3 k_d = diffuseColor * (1.0 - fss_ess + fms_ems);
    return (fms_ems + k_d) * irradiance;
}

float3 IblRadianceGgx(float3 N, float3 V, float roughness, float3 f0)
{
    float NoV = clamp(dot(N, V), 0.0, 1.0);

    float2 brdf_sample_point = clamp(float2(NoV, roughness), 0.0, 1.0);
    float2 f_ab = sampleLutGgx(brdf_sample_point).rg;

    float3 R = normalize(reflect(-V, N));
    float3 specular_light = sampleSpecularPmrem(R, roughness).rgb;

	// see https://bruop.github.io/ibl/#single_scattering_results at Single Scattering Results
	// Roughness dependent fresnel, from Fdez-Aguera
    float3 fr = max(1.0 - roughness, f0) - f0;
    float3 k_s = f0 + fr * pow(1.0 - NoV, 5.0);
    float3 fss_ess = k_s * f_ab.x + f_ab.y;

    return specular_light * fss_ess;
}

#endif 