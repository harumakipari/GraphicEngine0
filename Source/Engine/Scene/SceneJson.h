#pragma once

#include <json.hpp>

#include "SceneSetting.h"
#include "SceneState.h"

using json = nlohmann::json;

namespace DirectX
{
    inline void to_json(nlohmann::json& j, const XMFLOAT4& v)
    {
        j = { v.x, v.y, v.z, v.w };
    }

    inline void from_json(const nlohmann::json& j, XMFLOAT4& v)
    {
        v = XMFLOAT4(j[0], j[1], j[2], j[3]);
    }
    inline void to_json(nlohmann::json& j, const XMFLOAT3& v)
    {
        j = { v.x, v.y, v.z };
    }

    inline void from_json(const nlohmann::json& j, XMFLOAT3& v)
    {
        v = XMFLOAT3(j[0], j[1], j[2]);
    }
}

// SharedLightParam
inline void to_json(nlohmann::json& j, const SharedLightParam& s)
{
    j = {
        {"color", s.color},
        {"range", s.range}
    };
}

inline void from_json(const nlohmann::json& j, SharedLightParam& s)
{
    j.at("color").get_to(s.color);
    j.at("range").get_to(s.range);
}

// SceneLightConstants
inline void to_json(nlohmann::json& j, const SceneLightConstants& s)
{
    j = {
        {"lightDirection", s.lightDirection},
        {"lightColor", s.lightColor},

        {"iblIntensity", s.iblIntensity},
        {"directionalLightEnable", s.directionalLightEnable},
        {"pointLightEnable", s.pointLightEnable},
        {"pointLightCount", s.pointLightCount},

        {"rimColor", s.rimColor},
        {"rimIntensity", s.rimIntensity},

        {"playerRimColor", s.playerRimColor},
        {"playerRimIntensity", s.playerRimIntensity},

        {"playerHairRimColor", s.playerHairRimColor},
        {"playerHairRimIntensity", s.playerHairRimIntensity},

        {"rimPower", s.rimPower},
        {"kc", s.kc},
        {"kl", s.kl},
        {"kq", s.kq},

        {"diffuseIntensity", s.diffuseIntensity},
        {"specularIntensity", s.specularIntensity},
        {"pointLightDiffuseIntensity", s.pointLightDiffuseIntensity},
        {"pointLightSpecularIntensity", s.pointLightSpecularIntensity}
    };
}

inline void from_json(const nlohmann::json& j, SceneLightConstants& s)
{
    j.at("lightDirection").get_to(s.lightDirection);
    j.at("lightColor").get_to(s.lightColor);

    j.at("iblIntensity").get_to(s.iblIntensity);
    j.at("directionalLightEnable").get_to(s.directionalLightEnable);
    j.at("pointLightEnable").get_to(s.pointLightEnable);
    j.at("pointLightCount").get_to(s.pointLightCount);

    j.at("rimColor").get_to(s.rimColor);
    j.at("rimIntensity").get_to(s.rimIntensity);

    if (j.contains("playerRimColor")) j.at("playerRimColor").get_to(s.playerRimColor);
    if (j.contains("playerRimIntensity")) j.at("playerRimIntensity").get_to(s.playerRimIntensity);

    if (j.contains("playerHairRimColor")) j.at("playerHairRimColor").get_to(s.playerHairRimColor);
    if (j.contains("playerHairRimIntensity")) j.at("playerHairRimIntensity").get_to(s.playerHairRimIntensity);

    j.at("rimPower").get_to(s.rimPower);
    j.at("kc").get_to(s.kc);
    j.at("kl").get_to(s.kl);
    j.at("kq").get_to(s.kq);

    j.at("diffuseIntensity").get_to(s.diffuseIntensity);
    j.at("specularIntensity").get_to(s.specularIntensity);
    j.at("pointLightDiffuseIntensity").get_to(s.pointLightDiffuseIntensity);
    j.at("pointLightSpecularIntensity").get_to(s.pointLightSpecularIntensity);
}

inline void to_json(nlohmann::json& j, const SceneLightSaveData& s)
{
    j = {
        {"sceneConstants", s.sceneConstants},
        {"sharedLights", s.sharedLights}
    };
}

inline void from_json(const nlohmann::json& j, SceneLightSaveData& s)
{
    j.at("sceneConstants").get_to(s.sceneConstants);
    j.at("sharedLights").get_to(s.sharedLights);
}

inline void to_json(nlohmann::json& j, const SceneShaderConstants& s)
{
    j = {
        {"shadowColor", s.shadowColor},
        {"shadowDepthBias", s.shadowDepthBias},
        {"slopeBias", s.slopeBias},
        {"splitU", s.splitU},

        {"hueShift", s.hueShift},
        {"saturation", s.saturation},
        {"brightness", s.brightness},
        {"contrast", s.contrast},

        {"focusDistance", s.focusDistance},
        {"dofNearRange", s.dofNearRange},
        {"dofRange", s.dofRange},
        {"dofBlurStrength", s.dofBlurStrength},

        {"objectIblIntensity", s.objectIblIntensity},
        {"renderStep", s.renderStep},
        {"enableToneMapping", s.enableToneMapping},
        {"enableSsao", s.enableSsao},

        {"enableCascadedShadowMaps", s.enableCascadedShadowMaps},
        {"enableSsr", s.enableSsr},
        {"enableFog", s.enableFog},
        {"enableBloom", s.enableBloom},

        {"enableBlur", s.enableBlur},
        {"enableDof", s.enableDof},
        {"colorizeCascadedLayer", s.colorizeCascadedLayer},
        {"toneMappingValue", s.toneMappingValue},

        {"colorMapRGB", s.colorMapRGB},
        {"pad3", s.pad3},
    };
}

// SceneShaderConstants
inline void from_json(const nlohmann::json& j, SceneShaderConstants& s)
{
#if 1
    if (j.contains("shadowColor")) j.at("shadowColor").get_to(s.shadowColor);
    if (j.contains("shadowDepthBias")) j.at("shadowDepthBias").get_to(s.shadowDepthBias);
    if (j.contains("slopeBias")) j.at("slopeBias").get_to(s.slopeBias);
    if (j.contains("splitU")) j.at("splitU").get_to(s.splitU);

    if (j.contains("hueShift")) j.at("hueShift").get_to(s.hueShift);
    if (j.contains("saturation")) j.at("saturation").get_to(s.saturation);
    if (j.contains("brightness")) j.at("brightness").get_to(s.brightness);
    if (j.contains("contrast")) j.at("contrast").get_to(s.contrast);

    if (j.contains("focusDistance")) j.at("focusDistance").get_to(s.focusDistance);
    if (j.contains("dofNearRange")) j.at("dofNearRange").get_to(s.dofNearRange);
    if (j.contains("dofRange")) j.at("dofRange").get_to(s.dofRange);
    if (j.contains("dofBlurStrength")) j.at("dofBlurStrength").get_to(s.dofBlurStrength);

    if (j.contains("objectIblIntensity")) j.at("objectIblIntensity").get_to(s.objectIblIntensity);
    if (j.contains("renderStep")) j.at("renderStep").get_to(s.renderStep);
    if (j.contains("enableToneMapping")) j.at("enableToneMapping").get_to(s.enableToneMapping);
    if (j.contains("enableSsao")) j.at("enableSsao").get_to(s.enableSsao);

    if (j.contains("enableCascadedShadowMaps")) j.at("enableCascadedShadowMaps").get_to(s.enableCascadedShadowMaps);
    if (j.contains("enableSsr")) j.at("enableSsr").get_to(s.enableSsr);
    if (j.contains("enableFog")) j.at("enableFog").get_to(s.enableFog);
    if (j.contains("enableBloom")) j.at("enableBloom").get_to(s.enableBloom);

    if (j.contains("enableBlur")) j.at("enableBlur").get_to(s.enableBlur);
    if (j.contains("enableDof")) j.at("enableDof").get_to(s.enableDof);
    if (j.contains("colorizeCascadedLayer")) j.at("colorizeCascadedLayer").get_to(s.colorizeCascadedLayer);
    if (j.contains("toneMappingValue")) j.at("toneMappingValue").get_to(s.toneMappingValue);

    if (j.contains("colorMapRGB")) j.at("colorMapRGB").get_to(s.colorMapRGB);
    if (j.contains("pad3")) j.at("pad3").get_to(s.pad3);
#else
    j.at("shadowColor").get_to(s.shadowColor);
    j.at("shadowDepthBias").get_to(s.shadowDepthBias);
    j.at("slopeBias").get_to(s.slopeBias);
    j.at("splitU").get_to(s.splitU);

    j.at("hueShift").get_to(s.hueShift);
    j.at("saturation").get_to(s.saturation);
    j.at("brightness").get_to(s.brightness);
    j.at("contrast").get_to(s.contrast);

    j.at("focusDistance").get_to(s.focusDistance);
    j.at("dofNearRange").get_to(s.dofNearRange);
    j.at("dofRange").get_to(s.dofRange);
    j.at("dofBlurStrength").get_to(s.dofBlurStrength);

    j.at("objectIblIntensity").get_to(s.objectIblIntensity);
    j.at("renderStep").get_to(s.renderStep);
    j.at("enableToneMapping").get_to(s.enableToneMapping);
    j.at("enableSsao").get_to(s.enableSsao);

    j.at("enableCascadedShadowMaps").get_to(s.enableCascadedShadowMaps);
    j.at("enableSsr").get_to(s.enableSsr);
    j.at("enableFog").get_to(s.enableFog);
    j.at("enableBloom").get_to(s.enableBloom);

    j.at("enableBlur").get_to(s.enableBlur);
    j.at("enableDof").get_to(s.enableDof);
    j.at("colorizeCascadedLayer").get_to(s.colorizeCascadedLayer);
    j.at("value0").get_to(s.value0);

    j.at("pad0").get_to(s.pad0);
    j.at("pad1").get_to(s.pad1);
    j.at("pad2").get_to(s.pad2);
    j.at("pad3").get_to(s.pad3);
#endif // 0

}

// CascadeShadow
inline void to_json(nlohmann::json& j, const CascadedShadowMapConstants& s)
{
    j = {
        {"criticalDepthValue", s.criticalDepthValue},
        {"splitSchemeWeight", s.splitSchemeWeight},
        {"fitToCascade", s.fitToCascade},
        {"zDepthScale", s.zDepthScale},
    };
}

inline void from_json(const nlohmann::json& j, CascadedShadowMapConstants& s)
{
    j.at("criticalDepthValue").get_to(s.criticalDepthValue);
    j.at("splitSchemeWeight").get_to(s.splitSchemeWeight);
    j.at("fitToCascade").get_to(s.fitToCascade);
    j.at("zDepthScale").get_to(s.zDepthScale);
}

// FOG
inline void to_json(nlohmann::json& j, const FogConstants& s)
{
    j = {
        {"fogColor", s.fogColor},
        {"fogDensity", s.fogDensity},
        {"fogHeightFalloff", s.fogHeightFalloff},
        {"groundLevel", s.groundLevel},
        {"fogCutoffDistance", s.fogCutoffDistance},
        {"mieScatteringFactor", s.mieScatteringFactor},
        {"timeScale", s.timeScale},
        {"noiseScale", s.noiseScale},
        {"enableDither", s.enableDither},
        {"globalFogIntensity", s.globalFogIntensity},
        {"isWindowFog", s.isWindowFog},
    };
}

inline void from_json(const nlohmann::json& j, FogConstants& s)
{
    j.at("fogColor").get_to(s.fogColor);
    j.at("fogDensity").get_to(s.fogDensity);
    j.at("fogHeightFalloff").get_to(s.fogHeightFalloff);
    j.at("groundLevel").get_to(s.groundLevel);
    j.at("fogCutoffDistance").get_to(s.fogCutoffDistance);
    j.at("mieScatteringFactor").get_to(s.mieScatteringFactor);
    j.at("timeScale").get_to(s.timeScale);
    j.at("noiseScale").get_to(s.noiseScale);
    j.at("enableDither").get_to(s.enableDither);
    j.at("globalFogIntensity").get_to(s.globalFogIntensity);
    if (j.contains("isWindowFog")) j.at("isWindowFog").get_to(s.isWindowFog);

}

// SSR
inline void to_json(nlohmann::json& j, const SSRConstantBuffer& s)
{
    j = {
        {"reflectionIntensity", s.reflectionIntensity},
        {"maxDistance", s.maxDistance},
        {"resolution", s.resolution},
        {"steps", s.steps},
        {"thickness", s.thickness},
    };
}

inline void from_json(const nlohmann::json& j, SSRConstantBuffer& s)
{
    j.at("reflectionIntensity").get_to(s.reflectionIntensity);
    j.at("maxDistance").get_to(s.maxDistance);
    j.at("resolution").get_to(s.resolution);
    j.at("steps").get_to(s.steps);
    j.at("thickness").get_to(s.thickness);
}

// SSAO
inline void to_json(nlohmann::json& j, const SSAOConstantBuffer& s)
{
    j = {
        {"radius", s.radius},
        {"bias", s.bias},
        {"power", s.power}
    };
}

inline void from_json(const nlohmann::json& j, SSAOConstantBuffer& s)
{
    j.at("radius").get_to(s.radius);
    j.at("bias").get_to(s.bias);
    j.at("power").get_to(s.power);
}

// Bloom
inline void to_json(json& j, const BloomConstantBuffer& b)
{
    j = {
        {"bloomExtractionThreshold", b.bloomExtractionThreshold},
        {"bloomIntensity", b.bloomIntensity}
    };
}

inline void from_json(const json& j, BloomConstantBuffer& b)
{
    j.at("bloomExtractionThreshold").get_to(b.bloomExtractionThreshold);
    j.at("bloomIntensity").get_to(b.bloomIntensity);
}


// --- ActorTransformState ---
inline void to_json(json& j, const ActorTransformState& a)
{
    j = {
        {"name", a.name},
        {"position", {a.position.x, a.position.y, a.position.z}},
        {"rotation", {a.rotation.x, a.rotation.y, a.rotation.z, a.rotation.w}},
        {"scale", {a.scale.x, a.scale.y, a.scale.z}}
    };
}

inline void from_json(const json& j, ActorTransformState& a)
{
    a.name = j.at("name").get<std::string>();
    a.position.x = j.at("position")[0];
    a.position.y = j.at("position")[1];
    a.position.z = j.at("position")[2];
    a.rotation.x = j.at("rotation")[0];
    a.rotation.y = j.at("rotation")[1];
    a.rotation.z = j.at("rotation")[2];
    a.rotation.w = j.at("rotation")[3];
    if (j.contains("scale"))
    {
        a.scale.x = j.at("scale")[0];
        a.scale.y = j.at("scale")[1];
        a.scale.z = j.at("scale")[2];
    }
}

// --- CameraState ---
inline void to_json(json& j, const CameraState& c)
{
    j = {
        {"position", {c.position.x, c.position.y, c.position.z}},
        {"rotation", {c.rotation.x, c.rotation.y, c.rotation.z, c.rotation.w}},
        {"yaw", c.yaw},
        {"pitch", c.pitch},
        {"fov", c.fov},
    };
}

inline void from_json(const json& j, CameraState& c)
{
    c.position.x = j.at("position")[0];
    c.position.y = j.at("position")[1];
    c.position.z = j.at("position")[2];
    c.rotation.x = j.at("rotation")[0];
    c.rotation.y = j.at("rotation")[1];
    c.rotation.z = j.at("rotation")[2];
    c.rotation.w = j.at("rotation")[3];
    c.yaw = j.at("yaw");
    c.pitch = j.at("pitch");
    c.fov = j.at("fov");
}

inline void to_json(nlohmann::json& j, const CameraBookmark& b)
{
    j["name"] = b.name;
    j["position"] = { b.position.x, b.position.y, b.position.z };
    j["rotation"] = { b.rotation.x, b.rotation.y, b.rotation.z, b.rotation.w };
    j["yaw"] = b.yaw;
    j["pitch"] = b.pitch;
    j["fov"] = b.fov;
}

inline void from_json(const nlohmann::json& j, CameraBookmark& b)
{
    b.name = j.value("name", "Bookmark");
    b.position.x = j["position"][0];
    b.position.y = j["position"][1];
    b.position.z = j["position"][2];
    b.rotation.x = j["rotation"][0];
    b.rotation.y = j["rotation"][1];
    b.rotation.z = j["rotation"][2];
    b.rotation.w = j["rotation"][3];
    b.yaw = j.value("yaw", 0.0f);
    b.pitch = j.value("pitch", 0.0f);
    b.fov = j.value("fov", 0.0f);
}



inline void to_json(json& j, const SceneState& s)
{
    j = {
        {"lightSaveData", s.lightSaveData},
        {"shader", s.shader},
        {"cascadeShadow", s.cascadeShadow},
        {"fog", s.fog},
        {"ssr", s.ssr},
        {"ssao", s.ssao},
        {"bloom", s.bloom},
        {"camera", s.camera},
        {"cameraBookmarks", s.cameraBookmarks},
        {"actorStates", s.actorStates}
    };
}
inline void from_json(const json& j, SceneState& s)
{
#if 0
    j.at("lightSaveData").get_to(s.lightSaveData);
    j.at("shader").get_to(s.shader);
    j.at("cascadeShadow").get_to(s.cascadeShadow);
    j.at("fog").get_to(s.fog);
    j.at("ssr").get_to(s.ssr);
    j.at("ssao").get_to(s.ssao);
    j.at("bloom").get_to(s.bloom);

#else
    j.at("lightSaveData").get_to(s.lightSaveData);
    j.at("shader").get_to(s.shader);
    j.at("cascadeShadow").get_to(s.cascadeShadow);
    j.at("fog").get_to(s.fog);
    j.at("ssr").get_to(s.ssr);
    j.at("ssao").get_to(s.ssao);
    j.at("bloom").get_to(s.bloom);
    j.at("camera").get_to(s.camera);
    j.at("cameraBookmarks").get_to(s.cameraBookmarks);
    j.at("actorStates").get_to(s.actorStates);
#endif // 0
}




void SaveSceneState(const std::string& path, SceneState& state)
{
    nlohmann::json j = state;

    std::ofstream ofs(path);
    ofs << j.dump(4); // © 4‚Í®Œ`
}

void LoadSceneState(const std::string& path, SceneState& state)
{
    std::ifstream ifs(path);
    nlohmann::json j;
    ifs >> j;

    state = j.get<SceneState>();
}
