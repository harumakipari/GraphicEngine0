#pragma once
#include <fstream>
#include <json.hpp>

#include "Engine/Scene/SceneState.h"

class SceneEditor
{
public:
    static void Draw();

    static void LoadPresetList();

    static void LoadSceneState(const std::string& path, SceneState& state);

private:
    static inline std::vector<std::string> presetFiles;
};
