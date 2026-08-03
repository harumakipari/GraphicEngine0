#include "pch.h"
#include "LightManager.h"
#include "Engine/Scene/Scene.h"
#include "Components/Render/PointLightComponent.h"
#include <string>

#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#include "imgui.h"
#endif

struct AttenuationPreset
{
    float distance;
    float kc;
    float kl;
    float kq;
};

static AttenuationPreset presets[] =
{
    {7,    1.0f, 0.7f,   1.8f},
    {13,   1.0f, 0.35f,  0.44f},
    {20,   1.0f, 0.22f,  0.2f},
    {32,   1.0f, 0.14f,  0.07f},
    {50,   1.0f, 0.09f,  0.032f},
    {65,   1.0f, 0.07f,  0.017f},
    {100,  1.0f, 0.045f, 0.0075f},
    {160,  1.0f, 0.027f, 0.0028f},
    {200,  1.0f, 0.022f, 0.0019f},
    {325,  1.0f, 0.014f, 0.0007f},
    {600,  1.0f, 0.007f, 0.0002f},
    {3250, 1.0f, 0.0014f,0.000007f},
};


const std::vector<std::string> lightDisplayOrder =
{
    "TopCandelabra",
    "SideCandelabra",
    "MainChandelier",
    "CandleChandelier",
    "BrazierCenterBig",
    "BrazierCenterSmall",
    "GroundBrazierLight",
    "BottomStandingBrazier",
    "TopStandingBrazier",
    "MeltedWaxLight",
    "FireBowl",
    "TorchSconce",
    "CandleStand",
    "TorchLight",
    "WallLight",
    "LanternLight",
    "BossRoomPointLight",
    "MainRoomPointLight",
    "PlayerPointLight",
    "PlayerBackPointLight",
    "EnemyPointLight",
    "EnemyBackPointLight",
    "TitleRoomLight",
};


std::unordered_map<std::string, const char*> lightDisplayNames =
{
    {"MainChandelier", U8("メインシャンデリア")},
    {"CandleChandelier", U8("キャンドルシャンデリア")},
    {"BossRoomPointLight", U8("ボスの部屋のライト")},
    {"MainRoomPointLight", U8("メインの部屋のライト")},
    {"TopCandelabra", U8("燭台 上")},
    {"SideCandelabra", U8("燭台 左右")},
    {"BrazierCenterBig", U8("かご 火鉢 大")},
    {"BrazierCenterSmall", U8("かご 火鉢 小")},
    {"GroundBrazierLight", U8("三角形 火鉢")},
    {"MeltedWaxLight", U8("溶けた蝋")},
    {"BottomStandingBrazier", U8("スタンド火鉢 下")},
    {"TopStandingBrazier", U8("スタンド火鉢 上")},
    {"PlayerPointLight", U8("プレイヤーライト")},
    {"PlayerBackPointLight", U8("プレイヤーの後ろライト")},
    {"EnemyPointLight", U8("敵のライト")},
    {"EnemyBackPointLight", U8("敵の後ろライト")},
    {"FireBowl", U8("お椀の火")},
    {"TorchSconce", U8("たいまつの火")},
    {"CandleStand", U8("蝋燭スタンド")},
    {"TorchLight", U8("ボスの部屋のたいまつ")},
    {"WallLight", U8("ボスの部屋の壁のライト")},
    {"LanternLight", U8("ランタンのライト")},
    {"TitleRoomLight", U8("タイトル部屋のライト")},
};

void LightManager::Initialize(ID3D11Device* device)
{
    _ASSERT_EXPR(device != nullptr, L"Device is null in LightManager::Initialize");
    lightCBuffer = std::make_unique<ConstantBuffer<LightConstants>>(device);
    // 定数バッファの初期化
    constants = {};
    lightCBuffer->data = constants;
    renderPointLights.clear();
    scenePointLights.clear();
    auto& lightData = Scene::GetCurrentScene()->GetSceneSettings().sceneLightSaveData;
    InitializeDefaultLights(lightData.sharedLights);
}

SharedLightParam LightManager::FindSharedLight(const std::string& name)
{
    auto& lightData = Scene::GetCurrentScene()->GetSceneSettings().sceneLightSaveData;
    auto& sharedLights = lightData.sharedLights;

    auto it = sharedLights.find(name);
    if (it == sharedLights.end())
    {
        Logger::Error(U8("ライトがノードの名前と一致していません！"));
        return {};
    }

    return it->second;
}


void LightManager::SetDirectionalLight(Scene* scene, const DirectX::XMFLOAT4& dir, const DirectX::XMFLOAT4& color)
{
    auto& lightData = scene->GetSceneSettings().sceneLightSaveData;
    auto& light = lightData.sceneConstants;
    light.lightDirection = dir;
    light.lightColor = color;
}

void LightManager::InitializeDefaultLights(std::unordered_map<std::string, SharedLightParam>& sharedLights)
{
    // シャンデリアの共有ライト
    {
        sharedLights["MainChandelier"] =
            SharedLightParam{ DirectX::XMFLOAT4(1.0f, 0.584078431f, 0.254152089f, 2.4f),
        10.0f
        };

        sharedLights["CandleChandelier"] =
            SharedLightParam{ DirectX::XMFLOAT4(1.0f, 0.491020888f, 0.234550565f, 2.4f),
        1.5f
        };
    }

    // 燭台の共有ライト
    {
        sharedLights["TopCandelabra"] =
            SharedLightParam{
        DirectX::XMFLOAT4(1.0f, 0.577580452f, 0.309468925f, 3.5f),
        3.5f
        };

        sharedLights["SideCandelabra"] =
            SharedLightParam{
        DirectX::XMFLOAT4(1.0f, 0.577580452f, 0.309468925f, 1.2f),
        1.0f
        };
    }

    // 火鉢の共有ライト
    {
        sharedLights["BrazierCenterBig"] =
            SharedLightParam{
        DirectX::XMFLOAT4(1.0f, 0.533276379f, 0.258182853f, 1.44f),
        10.f
        };

        sharedLights["BrazierCenterSmall"] =
            SharedLightParam{ DirectX::XMFLOAT4(1.0f, 0.533276379f, 0.258182853f, 1.6f),
        8.0f
        };
    }

    // 地面の火鉢の共有ライト
    {
        sharedLights["GroundBrazierLight"] =
            SharedLightParam{ DirectX::XMFLOAT4(1.0f, 0.577580452f, 0.258182883f, 0.8f),
        10.f
        };
    }

    // 溶けた蝋の共有ライト
    {
        sharedLights["MeltedWaxLight"] =
            SharedLightParam{ DirectX::XMFLOAT4(1.0f, 0.630757093f, 0.219526187f, 7.28f),
        7.5f
        };
    }

    // ランタンの共有ライト
    {
        sharedLights["LanternLight"] =
            SharedLightParam{ DirectX::XMFLOAT4(1.0f, 0.630757093f, 0.219526187f, 7.28f),
        7.5f
        };
    }

    // タイトルの部屋の共有ライト
    {
        sharedLights["TitleRoomLight"] =
            SharedLightParam{ DirectX::XMFLOAT4(1.0f, 0.630757093f, 0.219526187f, 7.28f),
        7.5f
        };
    }

    // スタンド式火鉢の共有ライト
    {
        sharedLights["BottomStandingBrazier"] =
            SharedLightParam{ DirectX::XMFLOAT4(0.959999979f, 0.523895442f, 0.240151942f, 1.0f),
        8.0f
        };
        sharedLights["TopStandingBrazier"] =
            SharedLightParam{ DirectX::XMFLOAT4(1.0f, 0.545724452f, 0.25015831f, 1.6f),
        8.0f
        };
    }

    // プレイヤーのポイントライト
    {
        sharedLights["PlayerPointLight"] =
            SharedLightParam{ DirectX::XMFLOAT4(0.977f, 0.688f, 0.5f, 5.0f),
        3.1f
        };
        sharedLights["PlayerBackPointLight"] =
            SharedLightParam{ DirectX::XMFLOAT4(0.977f, 0.688f, 0.5f, 20.0f),
        3.1f
        };
    }

    // 部屋の共有ライト
    {
        // ボス
        sharedLights["BossRoomPointLight"] =
            SharedLightParam{ DirectX::XMFLOAT4(0.959999979f, 0.523895442f, 0.240151942f, 8.0f),
        8.0f
        };
        // メイン
        sharedLights["MainRoomPointLight"] =
            SharedLightParam{ DirectX::XMFLOAT4(0.959999979f, 0.523895442f, 0.240151942f, 8.0f),
        8.0f
        };
        // ボスの部屋のたいまつ
        sharedLights["TorchLight"] =
            SharedLightParam{ DirectX::XMFLOAT4(0.959999979f, 0.523895442f, 0.240151942f, 8.0f),
        8.0f
        };
        // ボスの部屋の壁のライト
        sharedLights["WallLight"] =
            SharedLightParam{ DirectX::XMFLOAT4(0.959999979f, 0.523895442f, 0.240151942f, 8.0f),
        8.0f
        };

    }

    // 蝋燭スタンドの共有ライト
    {
        sharedLights["CandleStand"] =
            SharedLightParam{ DirectX::XMFLOAT4(1.0f, 0.73791045f, 0.434153646f, 1.2f),
        8.0f
        };
    }

    // 敵のポイントライト
    {
        sharedLights["EnemyPointLight"] =
            SharedLightParam{ DirectX::XMFLOAT4(0.977f, 0.688f, 0.5f, 20.0f),
        3.1f
        };
        sharedLights["EnemyBackPointLight"] =
            SharedLightParam{ DirectX::XMFLOAT4(0.977f, 0.688f, 0.5f, 20.0f),
        3.1f
        };
    }

    // お椀の火のポイントライト
    {
        sharedLights["FireBowl"] =
            SharedLightParam{ DirectX::XMFLOAT4(1.0f, 0.752942204f, 0.527115107f, 3.0f),
        10.0f
        };
    }

    // たいまつの火のポイントライト
    {
        sharedLights["TorchSconce"] =
            SharedLightParam{ DirectX::XMFLOAT4(1.0f, 0.644479692f, 0.291770637f, 19.0f),
        20.0f
        };
    }

    // ゼロライト
    sharedLights["ZeroLight"] =
        SharedLightParam{ DirectX::XMFLOAT4(0.959999979f, 0.523895442f, 0.240151942f, 0.0f),
    8.0f
    };

}

void LightManager::Update(float deltaTime)
{
    auto& lightData = Scene::GetCurrentScene()->GetSceneSettings().sceneLightSaveData;
    auto& light = lightData.sceneConstants;

    auto& sharedLights = lightData.sharedLights;

    // 応急処置
    if (sharedLights.empty())
    {
        InitializeDefaultLights(sharedLights);
    }

    renderPointLights.clear();

    //Sceneライト
    for (auto& l : scenePointLights)
    {
        renderPointLights.push_back(l);
        if (renderPointLights.size() >= light.pointLightCount) break;
    }

    constants.lightDirection = light.lightDirection;
    constants.lightColor = light.lightColor;

    constants.iblIntensity = light.iblIntensity;
    constants.directionalLightEnable = static_cast<int>(light.directionalLightEnable);
    constants.pointLightEnable = static_cast<int>(light.pointLightEnable);
    constants.pointLightCount = light.pointLightCount;

    constants.rimColor = light.rimColor;
    constants.rimIntensity = light.rimIntensity;

    constants.playerHairRimColor = light.playerHairRimColor;
    constants.playerHairRimIntensity = light.playerHairRimIntensity;

    constants.playerRimColor = light.playerRimColor;
    constants.playerRimIntensity = light.playerRimIntensity;

    constants.rimPower = light.rimPower;
    constants.kc = light.kc;
    constants.kl = light.kl;
    constants.kq = light.kq;

    constants.diffuseIntensity = light.diffuseIntensity;
    constants.specularIntensity = light.specularIntensity;
    constants.pointLightDiffuseIntensity = light.pointLightDiffuseIntensity;
    constants.pointLightSpecularIntensity = light.pointLightSpecularIntensity;

    // デフォルト初期化
#if 1
    for (int i = 0; i < light.pointLightCount; i++)
    {
        constants.pointsLight[i] =
            (i < renderPointLights.size()) ? renderPointLights[i] : PointLight{};
    }
#endif // 1

#ifdef _DEBUG
#if 1
    if (showLightRange)
    {
        for (int i = 0; i < light.pointLightCount; i++)
        {
            auto& light = constants.pointsLight[i];

            float range = sqrt(1.0f / constants.kq);
            //DebugRender::DrawLightIcon(
            //    { light.position.x, light.position.y, light.position.z },
            //    light.color
            //);
            DebugRender::DrawSphere(
                { light.position.x,light.position.y, light.position.z },
                range,
                light.color
                , 0.0f, true
            );
        }
    }
#endif
#endif // _DEBUG
}

void LightManager::Apply(ID3D11DeviceContext* immediateContext, int slot) const
{
    lightCBuffer->data = constants;
    lightCBuffer->Activate(immediateContext, slot);
}

void LightManager::CollectPointLightsFromScene(const Scene& scene)
{
    scenePointLights.clear();
    auto& lightData = Scene::GetCurrentScene()->GetSceneSettings().sceneLightSaveData;
    auto& lightScene = lightData.sceneConstants;

    for (auto& actor : scene.GetActorManager()->GetAllActors())
    {
        std::vector<PointLightComponent*> components;
        actor->GetComponents<PointLightComponent>(components);
        for (auto& light : components)
        {
            if (!light->IsUsePointLight()) continue;
            scenePointLights.push_back(light->ToRenderLight());
            if (scenePointLights.size() >= lightScene.pointLightCount)
            {
                break;
            }
        }
    }
}

void LightManager::DrawGui()
{
#ifdef USE_IMGUI
    auto& lightData = Scene::GetCurrentScene()->GetSceneSettings().sceneLightSaveData;

    auto& light = lightData.sceneConstants;
    CheckboxInt(U8("平行光源 有効"), &light.directionalLightEnable);
    ImGui::DragFloat3(U8("ライト方向"), &light.lightDirection.x, 0.01f, -1.0f, 1.0f, "%.8f");
    ImGui::ColorEdit3(U8("ライト色"), &light.lightColor.x);
    ImGui::ColorEdit3(U8("リムライト色"), &light.rimColor.x);
    ImGui::SliderFloat(U8("リム強度"), &light.rimIntensity, 0.0f, 30.0f);
    ImGui::ColorEdit3(U8("プレイヤーのリムライト色"), &light.playerRimColor.x);
    ImGui::SliderFloat(U8("プレイヤーのリム強度"), &light.playerRimIntensity, 0.0f, 30.0f);
    ImGui::ColorEdit3(U8("プレイヤーの髪のリムライト色"), &light.playerHairRimColor.x);
    ImGui::SliderFloat(U8("プレイヤーの髪のリム強度"), &light.playerHairRimIntensity, 0.0f, 30.0f);
    ImGui::SliderFloat(U8("リムパワー"), &light.rimPower, 0.0f, 30.0f);
    ImGui::SliderFloat(U8("距離減衰"), &light.lightDirection.w, 0.0f, 1.0f);
    ImGui::SliderFloat(U8("Diffuse 強度"), &light.diffuseIntensity, 0.0f, 2.0f);
    ImGui::SliderFloat(U8("Specular 強度"), &light.specularIntensity, 0.0f, 2.0f);
    ImGui::SliderFloat(U8("ポイントライト Diffuse 強度"), &light.pointLightDiffuseIntensity, 0.0f, 2.0f);
    ImGui::SliderFloat(U8("ポイントライト Specular 強度"), &light.pointLightSpecularIntensity, 0.0f, 2.0f);
    ImGui::SliderFloat(U8("IBL 強度"), &light.iblIntensity, 0.0f, 20.0f);
    ImGui::SliderFloat(U8("ライト強度"), &light.lightColor.w, 0.0f, 20.0f);
    CheckboxInt(U8("ポイントライト 有効"), &light.pointLightEnable);
    ImGui::SliderInt(U8("ポイントライト数"), &light.pointLightCount, 0, PointLightMaxCount);
    ImGui::Checkbox(U8("ライト範囲表示"), &showLightRange);

    ImGui::SliderFloat("Kc", &light.kc, 0.0f, 2.0f);
    ImGui::SliderFloat("Kl", &light.kl, 0.0f, 1.0f);
    ImGui::SliderFloat("Kq", &light.kq, 0.0f, 2.0f);

    if (debugPointLights.size() != static_cast<size_t>(light.pointLightCount))
        debugPointLights.resize(light.pointLightCount); // 個数を合わせる

    if (ImGui::TreeNode(U8("共有ライト")))
    {
        auto& lightData = Scene::GetCurrentScene()->GetSceneSettings().sceneLightSaveData;
        auto& sharedLights = lightData.sharedLights;
#if 0
        for (auto& [name, light] : sharedLights)
        {
            const char* displayName = name.c_str();

            if (lightDisplayNames.contains(name))
            {
                displayName = lightDisplayNames[name];
            }
            if (ImGui::TreeNodeEx(displayName, ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::ColorEdit3(U8("色"), &light.color.x);
                ImGui::SliderFloat(U8("強度"), &light.color.w, 0.0f, 50.0f);
                ImGui::Combo(U8("ポイントライト距離"), &light.attenuationType,
                    "7\0"
                    "13\0"
                    "20\0"
                    "32\0"
                    "50\0"
                    "65\0"
                    "100\0"
                    "160\0"
                    "200\0"
                    "325\0"
                    "600\0"
                    "3250\0");
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
#else
        // 指定した順番で表示
        for (const std::string& name : lightDisplayOrder)
        {
            auto lightIt = sharedLights.find(name);

            // シーン内に存在しないライトは飛ばす
            if (lightIt == sharedLights.end())
                continue;

            auto& light = lightIt->second;

            const char* displayName = name.c_str();

            auto displayNameIt = lightDisplayNames.find(name);
            if (displayNameIt != lightDisplayNames.end())
            {
                displayName = displayNameIt->second;
            }

            // 内部名をIDとして使う
            ImGui::PushID(name.c_str());

            if (ImGui::TreeNodeEx(
                displayName,
                ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::ColorEdit3(
                    U8("色"),
                    &light.color.x);

                ImGui::SliderFloat(
                    U8("強度"),
                    &light.color.w,
                    0.0f,
                    50.0f);

                ImGui::Combo(
                    U8("ポイントライト距離"),
                    &light.attenuationType,
                    "7\0"
                    "13\0"
                    "20\0"
                    "32\0"
                    "50\0"
                    "65\0"
                    "100\0"
                    "160\0"
                    "200\0"
                    "325\0"
                    "600\0"
                    "3250\0");

                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        ImGui::TreePop();
#endif // 0

    }


#endif
}

void LightManager::LightGui()
{
    auto& lightData = Scene::GetCurrentScene()->GetSceneSettings().sceneLightSaveData;

    auto& light = lightData.sceneConstants;
    ImGui::SliderFloat(U8("Specular 強度"), &light.specularIntensity, 0.0f, 2.0f);

    if (debugPointLights.size() != static_cast<size_t>(light.pointLightCount))
        debugPointLights.resize(light.pointLightCount); // 個数を合わせる

    if (ImGui::TreeNodeEx(U8("共有ライト"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        auto& lightData = Scene::GetCurrentScene()->GetSceneSettings().sceneLightSaveData;
        auto& sharedLights = lightData.sharedLights;

        // 指定した順番で表示
        for (const std::string& name : lightDisplayOrder)
        {
            auto lightIt = sharedLights.find(name);

            // シーン内に存在しないライトは飛ばす
            if (lightIt == sharedLights.end())
                continue;

            auto& light = lightIt->second;

            const char* displayName = name.c_str();

            auto displayNameIt = lightDisplayNames.find(name);
            if (displayNameIt != lightDisplayNames.end())
            {
                displayName = displayNameIt->second;
            }

            // 内部名をIDとして使う
            ImGui::PushID(name.c_str());

            if (ImGui::TreeNodeEx(
                displayName,
                ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::ColorEdit3(
                    U8("色"),
                    &light.color.x);

                ImGui::SliderFloat(
                    U8("強度"),
                    &light.color.w,
                    0.0f,
                    50.0f);

                ImGui::Combo(
                    U8("ポイントライト距離"),
                    &light.attenuationType,
                    "7\0"
                    "13\0"
                    "20\0"
                    "32\0"
                    "50\0"
                    "65\0"
                    "100\0"
                    "160\0"
                    "200\0"
                    "325\0"
                    "600\0"
                    "3250\0");

                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        ImGui::TreePop();

    }
}