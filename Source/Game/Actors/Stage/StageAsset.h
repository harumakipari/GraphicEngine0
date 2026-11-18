#pragma once
#include <memory>
#include <vector>
#include "Graphics/Resource/InterleavedGltfModel.h"

struct StageAsset
{
    std::shared_ptr<InterleavedGltfModel> model;
    std::vector<InterleavedGltfModel::SpawnPoint> spawnPoints;
};
