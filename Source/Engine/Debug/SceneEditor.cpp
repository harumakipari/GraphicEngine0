#include "pch.h"
#include "SceneEditor.h"
#include <imgui.h>
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/SceneJson.h"
#include "Engine/Scene/SceneState.h"

void SceneEditor::Draw()
{
    static bool initialized = false;

    if (!initialized)
    {
        LoadPresetList();
        initialized = true;
    }

    //ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_Always);
    ImGui::Begin("SceneTransition");
    ImGui::Text("Scenes");
    ImGui::Separator();

    for (const auto& sceneName : Scene::GetRegisteredSceneNames())
    {
        if (ImGui::Button(sceneName.c_str(), ImVec2(-1, 0)))
        {
            Scene::_transition(sceneName, {});
        }
    }

    ImGui::End();

    ImGui::Begin("Save Scene Preset");

    // --- 保存系 ---
    auto* scene = Scene::GetCurrentScene();
    static SceneState savedState;

    static char fileName[64] = "newPreset.json";

    ImGui::InputText("FileName", fileName, sizeof(fileName));

    if (ImGui::Button("Save Preset"))
    {
        savedState.Capture(Scene::GetCurrentScene());
        std::string path = "Data/Saves/ScenePresets/" + std::string(fileName);

        // .json が付いてなかったら追加
        if (path.find(".json") == std::string::npos)
        {
            path += ".json";
        }

        SaveSceneState(path, savedState);
        LoadPresetList(); // 更新
    }

    for (auto& file : presetFiles)
    {
        if (ImGui::Button(file.c_str()))
        {
            LoadSceneState("Data/Saves/ScenePresets/" + file, savedState);
            savedState.Apply(Scene::GetCurrentScene());

            // ロードしたファイル名を fileName にコピーしておく
            strncpy_s(fileName, file.c_str(), sizeof(fileName));
        }
    }

    ImGui::End();

}

void SceneEditor::LoadPresetList()
{
    presetFiles.clear();

    for (const auto& entry : std::filesystem::directory_iterator("Data/Saves/ScenePresets"))
    {
        if (entry.path().extension() == ".json")
        {
            presetFiles.push_back(entry.path().filename().string());
        }
    }
}