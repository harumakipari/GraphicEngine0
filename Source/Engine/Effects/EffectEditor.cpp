#include "pch.h"
#define NOMINMAX
#include "EffectEditor.h"
#include <filesystem>
#ifdef USE_IMGUI
#include <imgui.h>
#endif // USE_IMGUI
#include "Graphics/Core/Graphics.h"
#include "Engine/Utility/JsonFileHandler.h"
#include "Engine/Utility/Dialog.h"

void EffectEditor::Show()
{
    isOpen = true;
}

bool EffectEditor::IsOpen()
{
    return isOpen;
}

void EffectEditor::Initialize()
{
    // 初期化処理が必要ならここに追加
}


void EffectEditor::DrawGUI()
{
#ifdef USE_IMGUI
    if (isOpen)
    {
        ImGui::Begin("Effect Editor", &isOpen);

        // ロード・セーブ・適用・クリアボタン
        {
            // ロード・セーブボタン
            if (ImGui::Button("Load"))
            {
                if (EffectHandle handle = EffectManager::LoadEffectDataWithDialog() > -1)
                {
                    currentEffectHandle = handle;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Save")) { EffectManager::SaveEffectDataWithDialog(currentEffectHandle); }
            ImGui::SameLine(0, 30.0f);
            // クリアボタン
            if (ImGui::Button("Clear")) { EffectManager::ClearAll(); }

        }

        // エフェクトデータリスト
        if (ImGui::CollapsingHeader("Effect Data List", ImGuiTreeNodeFlags_Leaf))
        {
            ImGui::Dummy(ImVec2(0.0f, 3.0f)); // 少しスペースを空ける

            for (size_t emitterIndex = 0; emitterIndex < EffectManager::effectData.size(); ++emitterIndex)
            {
                auto& effect = EffectManager::effectData[emitterIndex];
                // 一意のIDをプッシュ
                ImGui::PushID(static_cast<int>(emitterIndex));

                ImGui::Dummy(ImVec2(0.0f, 2.0f)); // 少しスペースを空ける

                // クリック状態フラグ
                bool isClicked = false;

                // セレクタブルでエフェクト名を表示
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));

                ImGui::Dummy(ImVec2(2.0f, 0.0f)); // 少しスペースを空ける
                ImGui::SameLine();
                isClicked |= ImGui::Selectable(effect.name.c_str(), currentEffectHandle == emitterIndex);
                ImGui::PopStyleColor(4);

                // クリックしたら現在のエフェクトハンドルを更新
                if (isClicked)
                {
                    currentEffectHandle = static_cast<EffectHandle>(emitterIndex);
                }
#if 1
                // 右クリックで削除メニュー表示
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                {
                    ImGui::OpenPopup("EffectDataContextMenu");
                }
                if (ImGui::BeginPopup("EffectDataContextMenu"))
                {
#if 0
                    if (ImGui::MenuItem("Delete"))
                    {
                        EffectManager::effectData.erase(EffectManager::effectData.begin() + emitterIndex);
                        // 現在のエフェクトハンドルが削除された場合、無効にする
                        if (currentEffectHandle == static_cast<EffectHandle>(emitterIndex))
                        {
                            currentEffectHandle = -1;
                        }
                        ImGui::EndPopup();
                        ImGui::PopID();
                        break; // ループを抜けて再描画
                    }
#endif // 0
                    if (ImGui::MenuItem("Copy"))
                    {
                        // エフェクトデータをコピー（複製）
                        EffectManager::CopyEffectData(static_cast<EffectHandle>(emitterIndex));
                    }
                    ImGui::EndPopup();
                }
#endif // 0
                ImGui::PopID();
            }

            ImGui::Dummy(ImVec2(0.0f, 5.0f)); // 少しスペースを空ける

            // 新しいエフェクトデータ追加ボタン
            if (ImGui::Button("Add New Effect Data"))
            {
                currentEffectHandle = EffectManager::CreateEffectData();
                // 追加したエフェクトデータの名前を設定
                EffectManager::effectData[currentEffectHandle].name = "Effect " + std::to_string(currentEffectHandle);
            }
            ImGui::SameLine();
            // 全エフェクトデータクリアボタン
            if (ImGui::Button("Clear All Effect Data"))
            {
                EffectManager::ClearEffectData();
                currentEffectHandle = -1;
            }
        }

        ImGui::Dummy(ImVec2(0.0f, 5.0f)); // 少しスペースを空ける

        ImGui::Separator();
        ImGui::Dummy(ImVec2(0.0f, 5.0f)); // 少しスペースを空ける

        // エミッタエディタ
        {
            // 現在のエフェクトハンドルが有効かチェック
            if (currentEffectHandle < 0 || currentEffectHandle >= EffectManager::effectData.size())
            {
                ImGui::Text("No Emitter Data Selected.");
                ImGui::End();
                return;
            }
            // エミッタデータリストの参照
            auto& emitterDataList = EffectManager::effectData.at(currentEffectHandle).emitters;

            // エフェクト名編集
            char effectNameBuffer[256]{};
            if (!ImGui::IsItemEdited())
            {
                strncpy_s(effectNameBuffer, EffectManager::effectData.at(currentEffectHandle).name.c_str(), sizeof(effectNameBuffer));
            }
            ImGui::InputText("Effect Name", effectNameBuffer, sizeof(effectNameBuffer));
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                EffectManager::effectData.at(currentEffectHandle).name = effectNameBuffer;
            }
            // エフェクト再生ボタン
            if (ImGui::Button("Play"))
            {
                EffectManager::Play(currentEffectHandle);
            }

            // 各エミッタデータ表示
            for (size_t i = 0; i < emitterDataList.size(); ++i)
            {
                // 一意のIDをプッシュ
                ImGui::PushID(static_cast<int>(i));

                auto& emitterData = emitterDataList[i];
                //if (ImGui::TreeNode((void*)(intptr_t)i, "Emitter %zu", i))
                ImGui::SetNextItemOpen(true, ImGuiCond_Once);
                if (ImGui::TreeNodeEx(emitterData.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    char nameBuffer[256]{};
                    if (!ImGui::IsItemEdited())
                    {
                        strncpy_s(nameBuffer, emitterData.name.c_str(), sizeof(nameBuffer));
                    }
                    ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer));
                    if (ImGui::IsItemDeactivatedAfterEdit())
                    {
                        emitterData.name = nameBuffer;
                    }
                    // テクスチャパスのバッファ
                    char texturePathBuffer[256]{};
                    // テクスチャパスの参照ボタン
                    if (ImGui::Button("Browse"))
                    {
                        // ファイルダイアログを開く
                        const char* filter = "Image Files\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0All Files\0*.*\0";
                        DialogResult result = Dialog::OpenFileName(texturePathBuffer, sizeof(texturePathBuffer), filter, "Select Texture", NULL, false);
                        if (result == DialogResult::OK)
                        {
                            emitterData.visualData.texturePath = texturePathBuffer;
                        }
                    }
                    ImGui::SameLine();
                    // テクスチャパスの入力欄
                    if (!ImGui::IsItemEdited())
                    {
                        strncpy_s(texturePathBuffer, emitterData.visualData.texturePath.c_str(), sizeof(texturePathBuffer));
                    }
                    ImGui::InputText("Texture Path", texturePathBuffer, sizeof(texturePathBuffer));
                    if (ImGui::IsItemDeactivatedAfterEdit())
                    {
                        emitterData.visualData.texturePath = texturePathBuffer;
                    }
                    ImGui::DragInt2("Texture Split", reinterpret_cast<int*>(&emitterData.visualData.textureSplitCount.x), 1, 1, 100);

                    // ビジュアル設定
                    {
                        const char* renderingModeItems[] = { "Billboard", "StretchedBillboard", "FixedRotation" };
                        int renderingModeIndex = static_cast<int>(emitterData.visualData.renderingMode);
                        if (ImGui::Combo("Rendering Mode", &renderingModeIndex, renderingModeItems, IM_ARRAYSIZE(renderingModeItems)))
                        {
                            emitterData.visualData.renderingMode = static_cast<EffectManager::RenderingMode>(renderingModeIndex);
                        }

                        const char* blendStateItems[] = { "Opaque", "Transparency", "Additive", "Subtraction", "Multiply" };
                        int blendStateIndex = static_cast<int>(emitterData.visualData.blendState);
                        if (ImGui::Combo("Blend State", &blendStateIndex, blendStateItems, IM_ARRAYSIZE(blendStateItems)))
                        {
                            emitterData.visualData.blendState = static_cast<BLEND_STATE>(blendStateIndex);
                        }
                    }

                    ImGui::DragInt("Max Particles", reinterpret_cast<int*>(&emitterData.emitData.maxParticles), 1, 1, 100000);
                    emitterData.emitData.maxParticles = (std::max)(1, emitterData.emitData.maxParticles);

                    // エミット設定
                    if (ImGui::TreeNode("Emit Settings"))
                    {
                        DrawRangeInt("Emit Count", emitterData.emitData.emitCount, 1, 1, emitterData.emitData.maxParticles);
                        DrawRangeFloat("Initial Delay", emitterData.emitData.initialDelay, 0.1f, 0.0f, 100.0f);
                        DrawRangeFloat("Emit Interval", emitterData.emitData.emitInterval, 0.1f, 0.0f, 100.0f);
                        DrawFloat("Emit Rate", emitterData.emitData.emitRate, 0.1f, 0.0f, 1000.0f);
                        DrawFloat("Emit LifeTime", emitterData.emitData.emitterLifeTime, 0.1f, 0.0f, 1000.0f);
                        DrawFloat("Emissive Power", emitterData.emitData.emissivePower, 1.0f, 0.0f, 30.0f);
                        ImGui::SliderInt("Emit burstCount", &emitterData.emitData.burstCount, 0, 50);
                        ImGui::Checkbox("Burst", &emitterData.emitData.isBurst);
                        ImGui::Checkbox("Loop", &emitterData.emitData.loop);
                        DrawVector3("Position", emitterData.emitData.positionOffset, 0.1f);
                        DrawRangeVector3("Rotation", emitterData.emitData.rotationEuler, 1.0f);
                        ImGui::TreePop();
                    }
                    // 形状エミッタ設定
                    if (ImGui::TreeNode("Shape Emitter Settings"))
                    {
                        const char* shapeTypeItems[] = { "Point", "Ring", "Sphere", "Cylinder" };
                        int shapeTypeIndex = static_cast<int>(emitterData.shapeData.shape);
                        if (ImGui::Combo("Shape Type", &shapeTypeIndex, shapeTypeItems, IM_ARRAYSIZE(shapeTypeItems)))
                        {
                            emitterData.shapeData.shape = static_cast<EffectManager::ShapeType>(shapeTypeIndex);
                        }
                        const char* directionModeItems[] = { "Default", "Axis", "Random", "Outward", "Inward", "Normal" };
                        int directionModeIndex = static_cast<int>(emitterData.shapeData.directionMode);
                        if (ImGui::Combo("Direction Mode", &directionModeIndex, directionModeItems, IM_ARRAYSIZE(directionModeItems)))
                        {
                            emitterData.shapeData.directionMode = static_cast<EffectManager::DirectionMode>(directionModeIndex);
                        }

                        // パラメータ表示マスク定義
                        static constexpr uint32_t None = 0;
                        static constexpr uint32_t DirectionAxis = 1 << 0;
                        static constexpr uint32_t Speed = 1 << 1;
                        static constexpr uint32_t Radius = 1 << 2;
                        static constexpr uint32_t Height = 1 << 3;
                        // 形状タイプごとのパラメータ表示マスク
                        static constexpr uint32_t shapeParamMask[] = {
                            /*ShapeType::Point*/ None,
                            /*ShapeType::Ring*/ Radius,
                            /*ShapeType::Sphere*/ Radius,
                            /*ShapeType::Cylinder*/ (Radius | Height),
                        };
                        // 方向モードごとのパラメータ表示マスク
                        static constexpr uint32_t directionParamMask[] = {
                            /*DirectionMode::Default*/ None,
                            /*DirectionMode::Axis*/ DirectionAxis | Speed,
                            /*DirectionMode::Random*/ Speed,
                            /*DirectionMode::Outward*/ Speed,
                            /*DirectionMode::Inward*/ Speed,
                            /*DirectionMode::Normal*/ Speed,
                        };
                        // 現在のエミッタ設定に基づく表示フラグ
                        uint32_t parameterFlags = shapeParamMask[static_cast<uint32_t>(emitterData.shapeData.shape)] |
                            directionParamMask[static_cast<uint32_t>(emitterData.shapeData.directionMode)];

                        if (parameterFlags & Speed)
                        {
                            DrawRangeFloat("Speed", emitterData.shapeData.speed, 0.1f, 0.0f, 100.0f);
                        }
                        if (parameterFlags & DirectionAxis)
                        {
                            DrawVector3("Direction Axis", emitterData.shapeData.directionAxis, 0.1f);
                        }
                        if (parameterFlags & Radius)
                        {
                            DrawFloat("Radius", emitterData.shapeData.radius, 0.1f, 0.0f, 100.0f);
                        }
                        if (parameterFlags & Height)
                        {
                            DrawFloat("Height", emitterData.shapeData.height, 0.1f, 0.0f, 100.0f);
                        }

                        ImGui::TreePop();
                    }

                    // 動作設定
                    if (ImGui::TreeNode("Motion Settings"))
                    {
                        DrawRangeVector3("Velocity", emitterData.motionData.velocity, 0.1f);
                        DrawRangeVector3("Acceleration", emitterData.motionData.acceleration, 0.1f);
                        DrawRangeFloat("Lifetime", emitterData.motionData.lifeTime, 0.1f, 0.0f, 100.0f);
                        ImGui::Checkbox("Use Gravity", &emitterData.motionData.useGravity);
                        ImGui::TreePop();
                    }

                    // ビジュアル設定
                    if (ImGui::TreeNode("Visual Settings"))
                    {
                        DrawRangeVector2("Start Size", emitterData.visualData.startSize, 0.1f);
                        DrawRangeVector2("End Size", emitterData.visualData.endSize, 0.1f);
                        DrawCurve("Size Curve", emitterData.visualData.sizeCurve, emitterData.visualData.dirty);
                        DrawRangeColor("Start Color", emitterData.visualData.startColor);
                        DrawRangeColor("End Color", emitterData.visualData.endColor);
                        ImGui::TreePop();
                    }

                    // エミッタ削除ボタン
                    if (ImGui::Button("Remove"))
                    {
                        emitterDataList.erase(emitterDataList.begin() + i);
                        ImGui::TreePop();
                        ImGui::PopID();
                        break;
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
                ImGui::Separator();
            }
            if (ImGui::Button("+", ImVec2(25, 25)))
            {
                auto& data = emitterDataList.emplace_back();
                data.name = "Emitter" + std::to_string(emitterDataList.size() - 1);
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear Emitters"))
            {
                emitterDataList.clear();
            }
        }

        ImGui::End();
    }
#endif // USE_IMGUI
}

#ifdef USE_IMGUI

bool EffectEditor::DrawRangeInt(const char* label, Range<int>& range, int speed, int min, int max)
{
    ImGui::PushID(label); // 一意のIDをプッシュ

    bool changed = false;
    ImGui::Text("%s", label);
    ImGui::SameLine();
    ImGui::PushItemWidth(70);

    changed |= ImGui::DragInt("##Min", &range.min, static_cast<float>(speed), min, range.max, "Min:%d");
    ImGui::SameLine();
    changed |= ImGui::DragInt("##Max", &range.max, static_cast<float>(speed), range.min, max, "Max:%d");
    ImGui::PopItemWidth();
    ImGui::PopID();

    return changed;
}

bool EffectEditor::DrawRangeUInt(const char* label, Range<unsigned int>& range, unsigned int speed, unsigned int min, unsigned int max)
{
    ImGui::PushID(label);
    bool changed = false;

    ImGui::Text("%s", label);
    ImGui::SameLine();
    ImGui::PushItemWidth(70);

    //uint32_t prevMin = range.min;
    //uint32_t prevMax = range.max;

    changed |= ImGui::DragScalar("##Min", ImGuiDataType_U32, &range.min, static_cast<float>(speed), &min, &range.max, "Min:%u");
    ImGui::SameLine();
    changed |= ImGui::DragScalar("##Max", ImGuiDataType_U32, &range.max, static_cast<float>(speed), &range.min, &max, "Max:%u");

    ImGui::PopItemWidth();
    ImGui::PopID();

#if 0
    // 整合性チェック
    if (range.min > range.max)
    {
        if (range.min != prevMin)
            range.max = range.min;
        else
            range.min = range.max;
        changed = true;
    }

    if (min == 0 && max == 0)
        return changed; // Clamp不要判定  


    // 範囲制限
    range.min = std::clamp(range.min, min, max);
    range.max = std::clamp(range.max, min, max);
#endif // 0

    return changed;
}

bool EffectEditor::DrawRangeFloat(const char* label, Range<float>& range, float speed, float min, float max)
{
    ImGui::PushID(label);
    bool changed = false;

    ImGui::Text("%s", label);
    ImGui::SameLine();
    ImGui::PushItemWidth(70);

#if 1
    float prevMin = range.min;
    float prevMax = range.max;
#endif // 0

    changed |= ImGui::DragFloat("##Min", &range.min, speed, min, range.max, "Min:%.3f");
    ImGui::SameLine();
    changed |= ImGui::DragFloat("##Max", &range.max, speed, range.min, max, "Max:%.3f");

    ImGui::PopItemWidth();
    ImGui::PopID();

#if 1
    // 整合性チェック
    if (range.min > range.max)
    {
        if (range.min != prevMin)
            range.max = range.min;
        else
            range.min = range.max;
        changed = true;
    }

    // Clamp不要判定
    float epsilon = 1e-6f;
    if (fabsf(min) < epsilon && fabsf(max) < epsilon)
        return changed; // Clamp不要

    // 範囲制限
    range.min = std::clamp(range.min, min, max);
    range.max = std::clamp(range.max, min, max);
#endif // 0


    return changed;
}

bool EffectEditor::DrawRangeVector2(const char* label, Range<Vector2>& range, float speed, float min, float max)
{
    bool changed = false;
    ImGui::PushID(label); // 一意のIDをプッシュ

    //Vector2 prevMin = range.min;
    //Vector2 prevMax = range.max;

    if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::PushItemWidth(140);
        // Minベクトル編集
        ImGui::Text("Min");
        ImGui::SameLine();
        changed |= ImGui::DragFloat2("##Min", reinterpret_cast<float*>(&range.min), speed, min, max, "%.2f");
        // Maxベクトル編集
        ImGui::Text("Max");
        ImGui::SameLine();
        changed |= ImGui::DragFloat2("##Max", reinterpret_cast<float*>(&range.max), speed, min, max, "%.2f");

        ImGui::PopItemWidth();
        ImGui::TreePop();
    }
    ImGui::PopID();

#if 0
    // 整合性チェック：軸ごと
    for (int i = 0; i < 2; ++i)
    {
        float* minPtr = (&range.min.x) + i;
        float* maxPtr = (&range.max.x) + i;
        float* prevMinPtr = (&prevMin.x) + i;

        if (*minPtr > *maxPtr)
        {
            if (*minPtr != *prevMinPtr)
                *maxPtr = *minPtr;
            else
                *minPtr = *maxPtr;
            changed = true;
        }

        float epsilon = 1e-6f;
        if (fabsf(min) < epsilon && fabsf(max) < epsilon)
            continue; // Clamp不要

        // Clamp
        *minPtr = std::clamp(*minPtr, min, max);
        *maxPtr = std::clamp(*maxPtr, min, max);
    }
#endif // 0


    return changed;
}

bool EffectEditor::DrawRangeVector3(const char* label, Range<Vector3>& range, float speed, float min, float max)
{
    bool changed = false;
    ImGui::PushID(label); // 一意のIDをプッシュ

    //Vector3 prevMin = range.min;
    //Vector3 prevMax = range.max;

    if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::PushItemWidth(210);
        // Minベクトル編集
        ImGui::Text("Min");
        ImGui::SameLine();
        changed |= ImGui::DragFloat3("##Min", reinterpret_cast<float*>(&range.min), speed, min, max, "%.2f");
        // Maxベクトル編集
        ImGui::Text("Max");
        ImGui::SameLine();
        changed |= ImGui::DragFloat3("##Max", reinterpret_cast<float*>(&range.max), speed, min, max, "%.2f");

        ImGui::PopItemWidth();
        ImGui::TreePop();
    }
    ImGui::PopID();

#if 0
    // 整合性チェック：軸ごと
    for (int i = 0; i < 3; ++i)
    {
        float* minPtr = (&range.min.x) + i;
        float* maxPtr = (&range.max.x) + i;
        float* prevMinPtr = (&prevMin.x) + i;

        if (*minPtr > *maxPtr)
        {
            if (*minPtr != *prevMinPtr)
                *maxPtr = *minPtr;
            else
                *minPtr = *maxPtr;
            changed = true;
        }

        float epsilon = 1e-6f;
        if (fabsf(min) < epsilon && fabsf(max) < epsilon)
            continue; // Clamp不要

        // Clamp
        *minPtr = std::clamp(*minPtr, min, max);
        *maxPtr = std::clamp(*maxPtr, min, max);
    }
#endif // 0


    return changed;
}

bool EffectEditor::DrawRangeColor(const char* label, Range<CoreColor>& range)
{
    bool changed = false;
    ImGui::PushID(label); // 一意のIDをプッシュ

    if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::PushItemWidth(200);
        // Minカラー編集
        ImGui::Text("Min");
        ImGui::SameLine();
        changed |= ImGui::ColorEdit4("##Min", &range.min.r);
        // Maxカラー編集
        ImGui::Text("Max");
        ImGui::SameLine();
        changed |= ImGui::ColorEdit4("##Max", &range.max.r);

        ImGui::PopItemWidth();
        ImGui::TreePop();
    }
    ImGui::PopID();

    return changed;
}

bool EffectEditor::DrawFloat(const char* label, float& value, float speed, float min, float max)
{
    ImGui::PushID(label); // 一意のIDをプッシュ
    bool changed = false;
    ImGui::PushItemWidth(200);
    ImGui::Text("%s", label);
    ImGui::SameLine();
    changed |= ImGui::DragFloat("##Value", &value, speed, min, max, "%.2f");
    ImGui::PopItemWidth();
    ImGui::PopID();
    return changed;
}

bool EffectEditor::DrawVector3(const char* label, Vector3& value, float speed, float min, float max)
{
    ImGui::PushID(label); // 一意のIDをプッシュ
    bool changed = false;
    ImGui::PushItemWidth(210);
    ImGui::Text("%s", label);
    ImGui::SameLine();
    changed |= ImGui::DragFloat3("##Value", reinterpret_cast<float*>(&value), speed, min, max, "%.2f");
    ImGui::PopItemWidth();
    ImGui::PopID();
    return changed;
}

void EffectEditor::DrawCurve(const char* label, FloatCurve& curve, bool& dirty)
{
    if (ImGui::TreeNode(label))
    {
        for (int i = 0; i < curve.points.size(); ++i)
        {
            ImGui::PushID(i);

            ImGui::DragFloat("Time", &curve.points[i].time, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Value", &curve.points[i].value, 0.01f, 0.0f, 5.0f);

            if (ImGui::Button("Delete"))
            {
                curve.points.erase(curve.points.begin() + i);
                ImGui::PopID();
                break;
            }

            ImGui::Separator();
            ImGui::PopID();
        }

        if (ImGui::Button("Add Point"))
        {
            curve.points.push_back({ 0.5f, 1.0f });
        }

        if (ImGui::Button("Apply"))
        {
            dirty = true; // ←ここで初めて反映
        }

        // カーブのプレビュー表示
        const int resolution = 64;
        static float values[resolution];

        for (int i = 0; i < resolution; ++i)
        {
            float t = (float)i / (resolution - 1);
            values[i] = curve.Evaluate(t);
        }

        ImGui::PlotLines("##CurvePreview", values, resolution, 0, nullptr, 0.0f, 2.0f, ImVec2(200, 80));

        ImGui::TreePop();
    }
}


#endif // USE_IMGUI
