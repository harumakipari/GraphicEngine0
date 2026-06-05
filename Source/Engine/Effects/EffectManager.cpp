#include "pch.h"
#include "EffectManager.h"

#include "Components/Base/SceneComponent.h"
#include "Engine/Debug/Assert.h"
#include "Graphics/Core/Graphics.h"
#include "Engine/Utility/JsonFileHandler.h"
#include "Engine/Utility/Dialog.h"
#include "Engine/Utility/JsonUtils.h"
#include "Graphics/Resource/Texture.h"
#include "Math/MathHelper.h"

void EffectManager::ClearAll()
{
    ClearEffectData();
    StopAll();
}

EffectHandle EffectManager::CreateEffectData()
{
    // 新しいエフェクトデータ追加用のハンドル
    EffectHandle handle = static_cast<EffectHandle>(effectData.size());
    // 空のエミッターデータを追加
    effectData.emplace_back();
    return handle;
}

EffectHandle EffectManager::LoadEffectData(const std::string& filePath)
{
    // エフェクトデータ読み込み
    if (std::filesystem::exists(filePath))
    {
        // すでに同じデータが存在する場合はそれを返す
        for (size_t i = 0; i < effectData.size(); ++i)
        {
            if (effectData[i].filePath == filePath)
            {
                return effectData[i].handle;
            }
        }

        json j;
        if (JsonFileHandler::LoadJsonFromFile(j, filePath))
        {
            // 新しいエフェクトデータ追加用のハンドル
            EffectHandle handle = CreateEffectData();
            effectData[handle].filePath = filePath;
            effectData[handle].handle = handle;

            // エフェクト名を反映
            effectData[handle].name = j.value("name", "Effect" + std::to_string(handle));

            // 読み込みデータをエミッターデータに反映
            auto& emitterDataList = effectData[handle].emitters;
            auto emitterJsonList = j.value("emitterDataList", json::array());
            for (const auto& emitterJson : emitterJsonList)
            {
                ParticleEmitterData emitterData;
                emitterData.name = emitterJson.value("name", "Emitter");

                // エミット設定
                {
                    emitterData.emitData.maxParticles = emitterJson.value("maxParticles", 1000);
                    if (emitterJson.contains("emitCount"))
                        emitterData.emitData.emitCount = emitterJson["emitCount"].get<Range<int>>();
                    else
                        emitterData.emitData.emitCount = { 10, 10 };
                    if (emitterJson.contains("initialDelay"))
                        emitterData.emitData.initialDelay = emitterJson["initialDelay"].get<Range<float>>();
                    else
                        emitterData.emitData.initialDelay = { 0.0f, 0.0f };
                    if (emitterJson.contains("emitInterval"))
                        emitterData.emitData.emitInterval = emitterJson["emitInterval"].get<Range<float>>();
                    else
                        emitterData.emitData.emitInterval = { 0.1f, 0.1f };
                    if (emitterJson.contains("positionOffset"))
                        emitterData.emitData.positionOffset = emitterJson["positionOffset"].get<Vector3>();
                    if (emitterJson.contains("rotationEuler"))
                        emitterData.emitData.rotationEuler = emitterJson["rotationEuler"].get<Range<Vector3>>();
                    if (emitterJson.contains("emitRate"))
                        emitterData.emitData.emitRate = emitterJson.value("emitRate", 10.0f);
                    if (emitterJson.contains("loop"))
                        emitterData.emitData.loop = emitterJson.value("loop", false);
                    if (emitterJson.contains("emitterLifeTime"))
                        emitterData.emitData.emitterLifeTime = emitterJson.value("emitterLifeTime", 0.5f);
                    if (emitterJson.contains("isBurst"))
                        emitterData.emitData.isBurst = emitterJson.value("isBurst", false);
                    if (emitterJson.contains("burstCount"))
                        emitterData.emitData.burstCount = emitterJson.value("burstCount", 10);
                    if (emitterJson.contains("emissivePower"))
                        emitterData.emitData.emissivePower = emitterJson.value("emissivePower", 1.0f);
                    //emitterData.emitData.loop = emitterJson.value("loop", false);
                }
                // 形状設定
                {
                    emitterData.shapeData.shape = static_cast<ShapeType>(emitterJson.value("shapeType", 0));
                    emitterData.shapeData.directionMode = static_cast<DirectionMode>(emitterJson.value("directionMode", 0));
                    if (emitterJson.contains("directionAxis"))
                        emitterData.shapeData.directionAxis = emitterJson["directionAxis"].get<Vector3>();
                    emitterData.shapeData.speed = emitterJson.value("speed", Range<float>{ 1.0f, 1.0f });
                    emitterData.shapeData.radius = emitterJson.value("radius", 1.0f);
                    emitterData.shapeData.height = emitterJson.value("height", 1.0f);
                }
                // 動作設定
                {
                    if (emitterJson.contains("velocity"))
                        emitterData.motionData.velocity = emitterJson["velocity"].get<Range<Vector3>>();
                    if (emitterJson.contains("acceleration"))
                        emitterData.motionData.acceleration = emitterJson["acceleration"].get<Range<Vector3>>();
                    if (emitterJson.contains("lifeTime"))
                        emitterData.motionData.lifeTime = emitterJson["lifeTime"].get<Range<float>>();
                    emitterData.motionData.useGravity = emitterJson.value("useGravity", false);
                }
                // ビジュアル設定
                {
                    emitterData.visualData.renderingMode = static_cast<RenderingMode>(emitterJson.value("renderingMode", 0));
                    emitterData.visualData.texturePath = emitterJson.value("texturePath", "");
                    auto textureSplitCountArray = emitterJson.value("textureSplitCount", std::vector<uint32_t>{ 1, 1 });
                    if (textureSplitCountArray.size() == 2) {
                        emitterData.visualData.textureSplitCount = { textureSplitCountArray[0], textureSplitCountArray[1] };
                    }
                    emitterData.visualData.blendState = static_cast<BLEND_STATE>(emitterJson.value("blendState", 0));
                    if (emitterJson.contains("startSize"))
                        emitterData.visualData.startSize = emitterJson["startSize"].get<Range<Vector2>>();
                    if (emitterJson.contains("endSize"))
                        emitterData.visualData.endSize = emitterJson["endSize"].get<Range<Vector2>>();
                    if (emitterJson.contains("startColor"))
                        emitterData.visualData.startColor = emitterJson["startColor"].get<Range<CoreColor>>();
                    if (emitterJson.contains("endColor"))
                        emitterData.visualData.endColor = emitterJson["endColor"].get<Range<CoreColor>>();

                }

                // エミッタデータリストに追加
                emitterDataList.push_back(emitterData);
            }

            // 成功したのでハンドルを返す
            return handle;
        }
    }
    // 読み込み失敗
    Logger::Error("Failed to load effect data from file: " + filePath);
    //_ASSERT_EXPR_A("Failed to load effect data from file: ");
    return -1; // 無効なハンドルを返す
}

EffectHandle EffectManager::LoadEffectDataWithDialog()
{
    // ファイルダイアログ表示
    {
        const char* filter = "JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
        char filePath[256] = { 0 };
        HWND hwnd = Graphics::GetHwnd();
        DialogResult result = Dialog::OpenFileName(filePath, sizeof(filePath), filter, nullptr, hwnd);
        if (result == DialogResult::OK) {
            std::filesystem::path path(filePath);
            // 拡張子が.jsonでない場合、.jsonを追加
            path.replace_extension(".json");

            // エフェクトデータ読み込み
            return LoadEffectData(path.string());
        }
        else {
            return -1; // 無効なハンドルを返す
        }
    }
}

void EffectManager::SaveEffectData(EffectHandle handle, const std::string& filePath)
{
    // エフェクトデータ保存
    json j;
    // エフェクト名保存
    j["name"] = effectData.at(handle).name;
    // エミッタデータリスト初期化
    j["emitterDataList"] = json::array();
    // エミッタデータ保存
    for (const auto& emitterData : effectData.at(handle).emitters)
    {
        // エミッターデータをJSONに変換
        json emitterJson;
        emitterJson["name"] = emitterData.name;
        // エミット設定
        {
            emitterJson["maxParticles"] = emitterData.emitData.maxParticles;
            emitterJson["emitCount"] = emitterData.emitData.emitCount;
            emitterJson["initialDelay"] = emitterData.emitData.initialDelay;
            emitterJson["emitInterval"] = emitterData.emitData.emitInterval;
            emitterJson["positionOffset"] = emitterData.emitData.positionOffset;
            emitterJson["rotationEuler"] = emitterData.emitData.rotationEuler;
            emitterJson["emitRate"] = emitterData.emitData.emitRate;
            emitterJson["emitterLifeTime"] = emitterData.emitData.emitterLifeTime;
            emitterJson["loop"] = emitterData.emitData.loop;
            emitterJson["emissivePower"] = emitterData.emitData.emissivePower;
            emitterJson["isBurst"] = emitterData.emitData.isBurst;
            emitterJson["burstCount"] = emitterData.emitData.burstCount;
        }
        // 形状設定
        {
            emitterJson["shapeType"] = static_cast<uint8_t>(emitterData.shapeData.shape);
            emitterJson["directionMode"] = static_cast<uint8_t>(emitterData.shapeData.directionMode);
            emitterJson["directionAxis"] = emitterData.shapeData.directionAxis;
            emitterJson["speed"] = emitterData.shapeData.speed;
            emitterJson["radius"] = emitterData.shapeData.radius;
            emitterJson["height"] = emitterData.shapeData.height;
        }
        // 動作設定
        {
            emitterJson["velocity"] = emitterData.motionData.velocity;
            emitterJson["acceleration"] = emitterData.motionData.acceleration;
            emitterJson["lifeTime"] = emitterData.motionData.lifeTime;
            emitterJson["useGravity"] = emitterData.motionData.useGravity;
        }
        // ビジュアル設定
        {
            emitterJson["renderingMode"] = static_cast<uint8_t>(emitterData.visualData.renderingMode);
            emitterJson["texturePath"] = emitterData.visualData.texturePath;
            emitterJson["textureSplitCount"] = { emitterData.visualData.textureSplitCount.x, emitterData.visualData.textureSplitCount.y };
            emitterJson["blendState"] = static_cast<uint8_t>(emitterData.visualData.blendState);
            emitterJson["startSize"] = emitterData.visualData.startSize;
            emitterJson["endSize"] = emitterData.visualData.endSize;
            emitterJson["startColor"] = emitterData.visualData.startColor;
            emitterJson["endColor"] = emitterData.visualData.endColor;
        }

        // エミッタデータリストに追加
        j["emitterDataList"].push_back(emitterJson);
    }
    // ファイルに保存
    JsonFileHandler::SaveJsonToFile(j, filePath);
}

void EffectManager::SaveEffectDataWithDialog(EffectHandle handle)
{
    // ファイルダイアログ表示
    {
        const char* filter = "JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
        char filePath[256] = { 0 };
        HWND hwnd = Graphics::GetHwnd();
        DialogResult result = Dialog::SaveFileName(filePath, sizeof(filePath), filter, nullptr, ".json", hwnd);
        if (result == DialogResult::OK) {
            std::filesystem::path path(filePath);
            // 拡張子が.jsonでない場合、.jsonを追加
            path.replace_extension(".json");
            // エフェクトデータ保存
            SaveEffectData(handle, path.string());
        }
    }
}

// エフェクト再生
void EffectManager::Play(EffectHandle handle, const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& rot)
{
    for (auto& emitterData : effectData[handle].emitters)
    {
        ActiveEmitter emitter;
        emitter.handle = handle;
        emitter.data = &emitterData;
        emitter.lifeTime = emitterData.emitData.emitterLifeTime;
        emitter.loop = emitterData.emitData.loop;
        emitter.position = pos;
        emitter.rotation = rot;
        emitter.emitAccumulator = 0.0f;
        emitter.hasBurst = false; // 一回だけBurstするために
        activeEmitters.push_back(emitter); // エミッタを起動
    }

    return;

}

void EffectManager::EmitParticle(EffectHandle handle, const XMFLOAT3& pos, const XMFLOAT3& rot)
{
    // ハンドルチェック
    if (handle < 0 || handle >= static_cast<EffectHandle>(effectData.size()))
    {
        //Console::LogError("EffectManager::Play: Invalid effect handle " + std::to_string(handle));
        return;
    }

    // 各エミッターデータを処理
    for (/*const*/ auto& emitterData : effectData[handle].emitters)
    {
        const std::string& effectFilePath = effectData[handle].filePath;
        // テクスチャパスと最大パーティクル数取得
        const std::string& texturePath = emitterData.visualData.texturePath;
        uint32_t maxParticles = emitterData.emitData.maxParticles;

        // 該当するパーティクルシステムが存在しない場合は生成
        if (particleSystems[effectFilePath].find(texturePath) == particleSystems[effectFilePath].end())
        {
            Texture texture;
            if (!texturePath.empty() && std::filesystem::exists(texturePath))
            {
                texture.LoadFromFile(texturePath);
            }
            else
            {
                texture.MakeDummy(Graphics::GetDevice());
            }
            //テクスチャSRV取得
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv = texture.GetSRV();
            //パーティクルシステム生成
            particleSystems[effectFilePath][texturePath] = std::make_unique<CoreComputeParticleSystem>(Graphics::GetDevice(), maxParticles, srv, emitterData.visualData.textureSplitCount);
        }
        // ブレンドステート設定
        particleSystems[effectFilePath][texturePath]->blendState = emitterData.visualData.blendState;

        // エミット数分ループ
        int emitCount = emitterData.emitData.emitCount.GetRandom();
        for (int i = 0; i < emitCount; ++i)
        {
            CoreComputeParticleSystem::EmitParticleData emitData;
            emitData.parameter.x = static_cast<float>(emitterData.visualData.renderingMode); // 描画モード設定
            emitData.parameter.y = emitterData.motionData.lifeTime.GetRandom(); // 生存時間設定

            float delayTime = 0.0f; // 遅延時間初期化
            delayTime += emitterData.emitData.initialDelay.GetRandom(); // 初期遅延時間設定
            delayTime += emitterData.emitData.emitInterval.GetRandom() * i; // エミット間隔設定

            emitData.parameter.z = delayTime; // 遅延時間設定
            //emitData.parameter.w = 0.0f; // テクスチャインデックスは未使用
            // 位置設定
            {
                emitData.position.x = pos.x;
                emitData.position.y = pos.y;
                emitData.position.z = pos.z;
            }

            // 回転設定
            {
                XMFLOAT3 euler = {
                    emitterData.emitData.rotationEuler.GetRandom().x + rot.x,
                    emitterData.emitData.rotationEuler.GetRandom().y + rot.y,
                    emitterData.emitData.rotationEuler.GetRandom().z + rot.z
                };
                emitData.rotation.x = DirectX::XMConvertToRadians(euler.x);
                emitData.rotation.y = DirectX::XMConvertToRadians(euler.y);
                emitData.rotation.z = DirectX::XMConvertToRadians(euler.z);
            }

            // 初速度設定
            {
                Vector3 velocity = emitterData.motionData.velocity.GetRandom();
                emitData.velocity.x = velocity.x;
                emitData.velocity.y = velocity.y;
                emitData.velocity.z = velocity.z;
            }

            // 加速度設定
            {
                Vector3 acceleration = emitterData.motionData.acceleration.GetRandom();
                if (emitterData.motionData.useGravity)
                {
                    acceleration += Vector3(0.0f, -9.81f, 0.0f); // 重力加速度を追加
                }
                emitData.acceleration = { acceleration.x, acceleration.y, acceleration.z, 0.0f };
            }

            // シェイプエミッタ設定を適用
            ApplyShapeEmitterSettings(emitterData.shapeData, emitData, i, emitCount);

            // その他設定
            {
                // 生成位置オフセット適用
                emitData.position.x += emitterData.emitData.positionOffset.x;
                emitData.position.y += emitterData.emitData.positionOffset.y;
                emitData.position.z += emitterData.emitData.positionOffset.z;
            }

            // ビジュアル設定
            {
                Vector2 startSize = emitterData.visualData.startSize.GetRandom();
                Vector2 endSize = emitterData.visualData.endSize.GetRandom();
                //emitData.scale = DirectX::XMFLOAT4(startSize.x, startSize.y, endSize.x, endSize.y);
                emitData.scale = DirectX::XMFLOAT4(startSize.x, startSize.y, endSize.x, endSize.y);
                CoreColor startColor = emitterData.visualData.startColor.GetRandom();
                CoreColor endColor = emitterData.visualData.endColor.GetRandom();
                emitData.startColor = startColor;
                emitData.endColor = endColor;
                emitData.customData.x = emitterData.emitData.emissivePower;
#if 1
                int curveIndex = emitterData.visualData.curveIndex;
                if (emitterData.visualData.dirty)
                {
                    int curveIndex = RegisterCurve(emitterData.visualData.sizeCurve);
                    emitterData.visualData.dirty = false;   // このためにconst取っている
                    emitData.customData.y = (float)curveIndex;
                }
                emitData.customData.y = static_cast<float>(curveIndex);
#else
                emitData.customData.y = (float)emitterData.visualData.curveIndex;
#endif // 0
            }

            // エミット
            particleSystems[effectFilePath][texturePath]->Emit(emitData);
        }
    }
}



// エフェクト再生（コンポーネントにアタッチ）
void EffectManager::PlayAttached(EffectHandle handle, const std::shared_ptr<SceneComponent>& target, bool followPosition, bool followRotation)
{
    if (!target) return;

    EffectAttachInfo info;
    info.handle = handle;
    info.target = target;
    info.followPosition = followPosition;
    info.followRotation = followRotation;

    info.emitInterval = 0.02f;   // ★必須
    info.emitTimer = 0.0f;

    info.lifeTime = 0.5f;        // ★必須（星が吸い込まれる時間）
    info.elapsed = 0.0f;



    attachedEffects.push_back(info);
}

EffectHandle EffectManager::CopyEffectData(EffectHandle srcHandle)
{
    // エフェクトデータコピー
    if (srcHandle < 0 || srcHandle >= static_cast<EffectHandle>(effectData.size()))
    {
        //Console::LogError("EffectManager::CopyEffectData: Invalid effect handle " + std::to_string(srcHandle));
        return -1; // 無効なハンドルを返す
    }
    // 新しいエフェクトデータ追加用のハンドル
    EffectHandle newHandle = CreateEffectData();
    // データコピー
    effectData[newHandle] = effectData[srcHandle];
    effectData[newHandle].handle = newHandle; // ハンドル更新
    effectData[newHandle].name += "_Copy"; // 名前更新
    effectData[newHandle].filePath += "_Copy"; // ファイルパス更新
    return newHandle;
}

EffectManager::EffectData& EffectManager::GetEffectData(EffectHandle handle)
{
    // エフェクトデータ取得
    if (handle < 0 || handle >= static_cast<EffectHandle>(effectData.size()))
    {
        throw std::out_of_range("EffectManager::GetEffectData: Invalid effect handle " + std::to_string(handle));
    }
    return effectData[handle];
}

void EffectManager::ClearEffectData()
{
    effectData.clear();
}

void EffectManager::StopAll()
{
    // 全エフェクト停止
    particleSystems.clear();
}

void EffectManager::Initialize()
{
    //パーティクルシステム初期化
    particleSystems.clear();
}

void EffectManager::Update(float deltaTime)
{
    bool curveDirty = false;

    for (auto& effect : effectData)
    {
        for (auto& emitter : effect.emitters)
        {
            if (emitter.visualData.dirty)
            {
                int index = RegisterCurve(emitter.visualData.sizeCurve);
                emitter.visualData.curveIndex = index; // ←追加！
                emitter.visualData.dirty = false;

                curveDirty = true;
            }
        }
    }

    if (curveDirty)
    {
        UpdateCurveTexture(); // GPU転送
    }

    for (auto it = activeEmitters.begin(); it != activeEmitters.end(); )
    {
        auto& emitter = *it;
        const auto& data = *emitter.data;

        emitter.elapsed += deltaTime;

        // Burst処理（最初の1回だけ）
        if (data.emitData.isBurst && !emitter.hasBurst)
        {
            for (int i = 0; i < data.emitData.burstCount; ++i)
            {
                EmitParticle(emitter.handle, emitter.position, emitter.rotation);
            }
            emitter.hasBurst = true;
        }

        // Rate処理
        if (data.emitData.emitRate > 0.0f)
        {
            emitter.emitAccumulator += deltaTime * data.emitData.emitRate;

            int emitCount = static_cast<int>(emitter.emitAccumulator);

            if (emitCount > 0)
            {
                emitter.emitAccumulator -= emitCount;

                for (int i = 0; i < emitCount; ++i)
                {
                    EmitParticle(emitter.handle, emitter.position, emitter.rotation);
                }
            }
        }

        // 寿命管理
        if (!emitter.loop && emitter.elapsed >= emitter.lifeTime)
        {
            it = activeEmitters.erase(it);
            continue;
        }

        ++it;
    }

    //パーティクルシステム更新
    ID3D11DeviceContext* immediateContext = Graphics::GetDeviceContext();

    for (auto& [effectFilePath, particleSystemList] : particleSystems)
    {
        for (auto& [texturePath, particleSystem] : particleSystemList)
        {
            particleSystem->Update(immediateContext, deltaTime);
        }
    }

    for (auto it = attachedEffects.begin(); it != attachedEffects.end(); )
    {
        auto& attached = *it;
        auto target = attached.target.lock();
        if (!target)
        {
            it = attachedEffects.erase(it);
            continue;
        }

        XMFLOAT3 pos = attached.followPosition
            ? target->GetComponentLocation()
            : XMFLOAT3{};

        XMFLOAT3 rot = attached.followRotation
            ? target->GetComponentEulerRotation()
            : XMFLOAT3{};

        attached.emitTimer += deltaTime;
        attached.elapsed += deltaTime;

        // ★ 一定間隔で Emit
        if (attached.emitTimer >= attached.emitInterval)
        {
            Play(attached.handle, pos, rot);
            attached.emitTimer = 0.0f;
        }

        // ★ 寿命で終了
        if (attached.elapsed >= attached.lifeTime)
        {
            it = attachedEffects.erase(it);
        }
        else
        {
            ++it;
        }
    }


}

void EffectManager::Render(ID3D11DeviceContext* immediateContext)
{
    if (curveArraySRV)
    {
        immediateContext->GSSetShaderResources(10, 1, curveArraySRV.GetAddressOf());
    }

    //パーティクルシステム描画
    for (auto& [effectFilePath, particleSystemList] : particleSystems)
    {
        for (auto& [texturePath, particleSystem] : particleSystemList)
        {
            // 描画前設定
            RenderState::BindBlendState(immediateContext, particleSystem->blendState);

            // 描画
            particleSystem->Render(immediateContext);
        }
    }
}


void EffectManager::SaveAllEffectData()
{
    // ファイルダイアログ表示
    {
        const char* filter = "JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";

        char filePath[256] = { 0 };
        DialogResult result = Dialog::SaveFileName(filePath, sizeof(filePath), filter);
        if (result == DialogResult::OK) {
            std::filesystem::path path(filePath);
            // 拡張子が.jsonでない場合、.jsonを追加
            path.replace_extension(".json");

            // エフェクトデータ保存
            for (EffectHandle handle = 0; handle < effectData.size(); ++handle)
            {
                SaveEffectData(handle, path.string());
            }
        }
        else {
            return; // キャンセルされた場合は保存を中止
        }
    }
}

void EffectManager::ReInitializeParticleSystem()
{
    // パーティクルシステム再初期化
    StopAll();
}

void EffectManager::ApplyShapeEmitterSettings(const EmitterShapeData& s, CoreComputeParticleSystem::EmitParticleData& emitData, int index, int emitCount)
{
    // シェイプエミッタ設定をemitDataに適用
    Vector3 position{};
    Vector3 velocityDir{};

    // 位置設定
    switch (s.shape)
    {
    case ShapeType::Point:
    {
        // 点の位置は原点
        position = Vector3(0.0f, 0.0f, 0.0f);
        break;
    }
    case ShapeType::Ring:
    {
        // リング上の均等な位置を取得
        float angle = (static_cast<float>(index) / emitCount) * DirectX::XM_2PI;
        position = Vector3(std::cosf(angle), 0.0f, std::sinf(angle)) * s.radius;
        break;
    }
    case ShapeType::Sphere:
    {
        // ランダムな球の中の位置を取得
        float u = Random(0.0f, 1.0f); // 0~1のランダムな値
        float v = Random(0.0f, 1.0f); // 0~1のランダムな値
        float theta = u * DirectX::XM_2PI; // 0~2PIのランダムな角度
        float phi = std::acosf(2.0f * v - 1.0f); // 0~PIのランダムな角度
        float r = s.radius * std::cbrtf(Random(0.0f, 1.0f)); // 半径を考慮したランダムな距離
        float x = r * std::sinf(phi) * std::cosf(theta);
        float y = r * std::sinf(phi) * std::sinf(theta);
        float z = r * std::cosf(phi);
        position = Vector3(x, y, z);
        break;
    }
    case ShapeType::Cylinder:
    {
        // ランダムな円柱の中の位置を取得
        float angle = Random(0.0f, DirectX::XM_2PI);
        float radius = Random(0.0f, s.radius);
        float height = Random(-s.height / 2.0f, s.height / 2.0f);
        float x = radius * std::cosf(angle);
        float z = radius * std::sinf(angle);
        position = Vector3(x, height, z);
        break;
    }
    }

    // 方向設定
    switch (s.directionMode)
    {
    case DirectionMode::Default:
    {
        velocityDir = Vector3(0, 0, 0); // 速度方向は設定しない
        break;
    }
    case DirectionMode::Axis:
    {
        // 指定軸方向を正規化して使用
        velocityDir = s.directionAxis.Normalize();
        break;
    }
    case DirectionMode::Random:
    {
        velocityDir = RandomDirection();
        break;
    }
    case DirectionMode::Outward:
    {
        // 位置ベクトルがゼロベクトルに近い場合の対策
        if (position.LengthSq() < 1e-6f)
        {
            // 位置ベクトルがゼロベクトルに近い場合、ランダム方向を使用
            velocityDir = RandomDirection();
        }
        else // 通常の場合
        {
            velocityDir = position.Normalize();
        }
        break;
    }
    case DirectionMode::Inward:
    {
        // 位置ベクトルがゼロベクトルに近い場合の対策
        if (position.LengthSq() < 1e-6f)
        {
            // 位置ベクトルがゼロベクトルに近い場合、方向はゼロベクトルにする
            velocityDir = Vector3(0.0f, 0.0f, 0.0f);
        }
        else // 通常の場合
        {
            velocityDir = (position * -1.0f).Normalize();
        }
        break;
    }
    case DirectionMode::Normal:
    {
        // 形状に応じた法線方向を設定
        switch (s.shape)
        {
        case ShapeType::Point:
        {
            velocityDir = Vector3(0.0f, 1.0f, 0.0f); // 上方向
            break;
        }
        case ShapeType::Ring:
        case ShapeType::Sphere:
        {
            // 位置ベクトルがゼロベクトルに近い場合の対策
            if (position.LengthSq() < 1e-6f)
            {
                velocityDir = RandomDirection();
            }
            else
            {
                velocityDir = position.Normalize();
            }
            break;
        }
        case ShapeType::Cylinder:
        {
            // 上下面に近い場合
            const float topThreshold = s.height * 0.45f;
            if (std::fabs(position.y) > topThreshold)
            {
                velocityDir = Vector3(0.0f, position.y > 0 ? 1.0f : -1.0f, 0.0f);
            }
            else
            {
                Vector3 radialDir = Vector3(position.x, 0.0f, position.z);
                velocityDir = radialDir.LengthSq() < 1e-6f ?
                    Vector3(0.0f, 1.0f, 0.0f) : radialDir.Normalize();
            }
            break;
        }
        }
        break;
    }
    }

    // 回転行列計算
    XMMATRIX rotMatrix = XMMatrixRotationRollPitchYawFromVector(XMLoadFloat4(&emitData.rotation));
    XMFLOAT3 localPos = position;
    XMVECTOR LocalPos = XMLoadFloat3(&localPos);

    // 位置回転
    XMVECTOR RotatedLocalPos = XMVector3Transform(LocalPos, rotMatrix);

    // ワールド位置計算
    XMFLOAT3 emitterWorldPos = { emitData.position.x, emitData.position.y, emitData.position.z };
    XMVECTOR EmitterWorldPos = XMLoadFloat3(&emitterWorldPos);
    XMVECTOR WorldPos = RotatedLocalPos + EmitterWorldPos;

    // 位置設定
    XMStoreFloat4(&emitData.position, WorldPos);

    // 速度ベクトル計算
    XMVECTOR dir = XMVector3TransformNormal(XMLoadFloat3(reinterpret_cast<XMFLOAT3*>(&velocityDir)), rotMatrix);
    XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(&velocityDir), dir);
    Vector3 velocity = velocityDir.Normalize() * s.speed.GetRandom();

    // 速度設定
    emitData.velocity.x += velocity.x;
    emitData.velocity.y += velocity.y;
    emitData.velocity.z += velocity.z;
}

float EffectManager::Random(float min, float max)
{
    // minからmaxまでのランダムな浮動小数点数を生成
    //return min + static_cast <float> (rand()) / (static_cast <float> (RAND_MAX / (max - min)));
    return MathHelper::RandomRange(min, max);
}

Vector3 EffectManager::RandomBoxPosition(const Vector3& size)
{
    float x = Random(-size.x / 2.0f, size.x / 2.0f);
    float y = Random(-size.y / 2.0f, size.y / 2.0f);
    float z = Random(-size.z / 2.0f, size.z / 2.0f);
    return Vector3(x, y, z);
}

Vector3 EffectManager::RandomDirection()
{
    // ランダムな単位ベクトルを生成
    // -1~1のランダムなz値
    float z = Random(-1.0f, 1.0f);
    // 0~2PIのランダムな角度
    float theta = Random(0.0f, DirectX::XM_2PI);
    // 半径を計算
    float r = std::sqrtf(1.0f - z * z);
    // x,y座標を計算
    float x = r * std::cosf(theta);
    float y = r * std::sinf(theta);

    return Vector3(x, y, z);
}

Vector3 EffectManager::RandomHemisphereDirection(const Vector3& normal)
{
    Vector3 dir = RandomDirection();
    if (dir.Dot(normal) < 0.0f)
    {
        dir = dir * -1.0f;
    }
    return dir.Normalize();
}

Vector3 EffectManager::RandomConeDirection(const Vector3& dir, float coneAngle)
{
    // 基準方向を上ベクトルにして回転行列を作成
    float cosTheta = Random(std::cosf(XMConvertToRadians(coneAngle)), 1.0f);
    float sinTheta = std::sqrtf(1.0f - cosTheta * cosTheta);
    float phi = Random(0.0f, DirectX::XM_2PI);

    Vector3 localDir{
        sinTheta * std::cosf(phi),
        cosTheta,
        sinTheta * std::sinf(phi),
    };

    // dir方向への回転行列を計算
    Vector3 axis = Vector3(Vector3::up).Cross(dir).Normalize();
    float angle = std::acosf(Vector3(Vector3::up).Dot(dir.Normalize()));

    // dirが上ベクトルとほぼ平行な場合、回転は不要
    if (axis.LengthSq() < 1e-6f)
    {
        return dir.Normalize();
    }

    // ロドリゲスの回転公式を使用して回転
    float cosA = std::cosf(angle);
    float sinA = std::sinf(angle);
    Vector3 rotatedDir =
        localDir * cosA +
        axis.Cross(localDir) * sinA +
        axis * (axis.Dot(localDir)) * (1 - cosA);
    return rotatedDir.Normalize();
}

void EffectManager::UpdateCurveTexture()
{
    if (curveData.empty()) return;

    int resolution = (int)curveData[0].size();
    int arraySize = (int)curveData.size();

    D3D11_TEXTURE1D_DESC desc{};
    desc.Width = resolution;
    desc.MipLevels = 1;
    desc.ArraySize = arraySize;
    desc.Format = DXGI_FORMAT_R32_FLOAT;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    std::vector<D3D11_SUBRESOURCE_DATA> initData(arraySize);

    for (int i = 0; i < arraySize; ++i)
    {
        initData[i].pSysMem = curveData[i].data();
    }

    Microsoft::WRL::ComPtr<ID3D11Texture1D> texture;
    Graphics::GetDevice()->CreateTexture1D(&desc, initData.data(), &texture);
    Graphics::GetDevice()->CreateShaderResourceView(texture.Get(), nullptr, &curveArraySRV);
}

// カーブ → 1Dテクスチャ化関数
int EffectManager::RegisterCurve(const FloatCurve& curve)
{
    const int resolution = 256;

    std::vector<float> samples(resolution);

    for (int i = 0; i < resolution; ++i)
    {
        float t = (float)i / (resolution - 1);
        samples[i] = curve.Evaluate(t);
    }

    int index = (int)curveData.size();
    curveData.push_back(samples);

    return index;
}
