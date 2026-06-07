#pragma once
#include <DirectXMath.h>
#include <vector>
#include <memory>

#include "Engine/Scene/SceneSetting.h"
#include "Graphics/Core/ConstantBuffer.h"

class Scene;


class LightManager
{
    const static inline int PointLightMaxCount = 100;
public:
    struct PointLight
    {
        DirectX::XMFLOAT4 position{ 0.0f,0.0f,0.0f,0.0f };
        DirectX::XMFLOAT4 color{ 1.0f,0.0f,0.0f,0.0f };
        float range = 0.0f;
        float pads[3] = {};
    };

    struct LightConstants
    {
        DirectX::XMFLOAT4 lightDirection = {};// w:attenuation Rate
        DirectX::XMFLOAT4 lightColor = {}; //w colorPower

        float iblIntensity ;
        int directionalLightEnable ;// 平行光源の on / off
        int pointLightEnable ;
        int pointLightCount ;

        DirectX::XMFLOAT3 rimColor = { 0.3f,0.5f,1.0f };
        float rimIntensity = 0.112f;

        float rimPower = 3.0f;
        float kc = 1.0f;
        float kl = 0.7f;
        float kq = 1.8f;

        float diffuseIntensity = 1.272f;
        float specularIntensity = 0.323f;
        float pointLightDiffuseIntensity = 0.9f;
        float pointLightSpecularIntensity = 0.15f;

        DirectX::XMFLOAT3 playerRimColor = { 0.3f,0.5f,1.0f };
        float playerRimIntensity = 0.112f;

        DirectX::XMFLOAT3 playerHairRimColor = { 0.3f,0.5f,1.0f };
        float playerHairRimIntensity = 0.112f;

        PointLight pointsLight[PointLightMaxCount];
    };

    SharedLightParam FindSharedLight(const std::string& name);
public:
    void Initialize(ID3D11Device* device);
    void Update(float deltaTime);
    void Apply(ID3D11DeviceContext* context, int slot) const;

    void AddPointLight(const PointLight& light)
    {
        renderPointLights.push_back(light);
    }

    void CollectPointLightsFromScene(const Scene& scene);

    void SetDirectionalLight(Scene* scene, const DirectX::XMFLOAT4& dir, const DirectX::XMFLOAT4& color);
    void InitializeDefaultLights(std::unordered_map<std::string, SharedLightParam>& sharedLights);

    void DrawGui();

    const DirectX::XMFLOAT4& GetLightDirection() const { return constants.lightDirection; }
private:
    bool showLightRange = false; // ポイントライトの範囲をデバッグ表示するか
    LightConstants constants = {};
    // GPUに送る最終のポイントライト情報
    std::vector<PointLight> renderPointLights;
    // ① デバッグ / 手動ライト（ImGui用）
    std::vector<PointLight> debugPointLights;
    // ② SceneComponent 由来ライト
    std::vector<PointLight> scenePointLights;

    std::unique_ptr<ConstantBuffer<LightConstants>> lightCBuffer;

};
