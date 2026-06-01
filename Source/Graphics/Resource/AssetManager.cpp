#include "pch.h"
#include "AssetManager.h"
#include "InterleavedGltfModel.h"

AssetManager& AssetManager::Get()
{
    static AssetManager instance;
    return instance;
}

std::shared_ptr<InterleavedGltfModel>
AssetManager::LoadModel(ID3D11Device* device, const std::string& filename, ModelTypes::ModelMode mode, bool isSaveVerticesData,bool convertToLHS )
{
    auto it = modelCache.find(filename);

    if (it != modelCache.end())
    {
        auto model = it->second.lock();

        if (model)
        {
            return model; // キャッシュ
        }
    }

    // 新規ロード
    auto model = std::make_shared<InterleavedGltfModel>(device, filename,mode, isSaveVerticesData, convertToLHS);

    modelCache[filename] = model;

    return model;
}


