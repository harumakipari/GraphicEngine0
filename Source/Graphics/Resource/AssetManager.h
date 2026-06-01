#pragma once
#include <unordered_map>
#include <memory>
#include <string>

#include "InterleavedGltfModel.h"
#include "ModelType.h"

struct AnimationAsset
{
    std::vector<InterleavedGltfModel::Animation> animations;
};
class AssetManager
{
public:
    static AssetManager& Get();

    std::shared_ptr<InterleavedGltfModel> LoadModel(ID3D11Device* device, const std::string& filename, ModelTypes::ModelMode mode, bool isSaveVerticesData = false, bool convertToLHS = false);

private:
    // モデルのキャッシュ。キーはファイルパス
    std::unordered_map<std::string, std::weak_ptr<InterleavedGltfModel>> modelCache;
};