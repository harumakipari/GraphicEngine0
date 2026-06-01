#pragma once
#include <fstream>
#include <json.hpp>

#include "Engine/Scene/SceneState.h"

class SceneEditor
{
public:
    static void Draw();

private:
    static void LoadPresetList();
    static inline std::vector<std::string> presetFiles;
};
