#include "pch.h"
//#include "ResourceManager.h"
//#include "Engine/Resources/Shader.h" // Shaderクラスの定義が必要
//#include "Engine/Resources/Texture.h" // Textureクラスの定義が必要
//#include "Engine/Editor/Console.h"
//
//void ResourceManager::Initialize()
//{
//	_files.clear();
//	_resources.clear();
//	shaderPaths.clear();
//
//#ifdef _DEBUG
//	// 初期化時にすべてのシェーダーファイルを読み込み
//	LoadAllShaders();
//#endif // _DEBUG
//}
//
//void ResourceManager::Finalize()
//{
//	UnloadAll();
//	_files.clear();
//	shaderPaths.clear();
//}
//
//std::shared_ptr<Resource> ResourceManager::Get(const std::string& path)
//{
//	auto it = _resources.find(path);
//	if (it != _resources.end()) {
//		return it->second;
//	}
//	return nullptr; // 見つからなかった場合
//}
//
//std::vector<std::string> ResourceManager::GetShaderPaths()
//{
//	return shaderPaths;
//}
//
//void ResourceManager::Reload(const std::string& path)
//{
//	auto it = _resources.find(path);
//	if (it != _resources.end()) {
//		it->second->Reload();
//	}
//}
//
//void ResourceManager::Unload(const std::string& path)
//{
//	auto it = _resources.find(path);
//	if (it != _resources.end()) {
//		it->second->ReleaseRef();
//		if (it->second->RefCount() <= 0) {
//			_resources.erase(it);
//		}
//	}
//}
//
//void ResourceManager::UnloadAll()
//{
//	for (auto& [path, resource] : _resources) {
//		resource->ReleaseRef();
//	}
//	_resources.clear();
//}
//
//void ResourceManager::Update()
//{
//#ifdef _DEBUG
//	UpdateHotReload();
//#endif // _DEBUG
//}
//
//void ResourceManager::UpdateHotReload()
//{
//#ifdef _DEBUG
//	// ホットリロードの実装例（ファイルの変更を監視してリロードするなど）
//	for (auto& [path, resource] : _resources)
//	{
//		// ファイルの存在チェック
//		if (!std::filesystem::exists(path)) continue;
//
//		// 最終更新日時の取得
//		auto& lastTime = _files[path];
//
//		// 例えば、ファイルのタイムスタンプをチェックして変更があればリロード
//		auto now = std::filesystem::last_write_time(path);
//
//#ifdef _DEBUG
//		// 追加: csoファイルの場合は対応するhlslファイルの更新もチェック
//		std::filesystem::path fsPath(path);
//		if (fsPath.extension() == ".cso") {
//			// csoファイルだったら、hlslに変換して登録
//			std::filesystem::path hlslPath = shaderDir / fsPath.filename().replace_extension(".hlsl");
//
//			// hlslファイルも監視対象に追加
//			if (std::filesystem::exists(hlslPath))
//			{
//				lastTime = _files[hlslPath.string()];
//				now = std::filesystem::last_write_time(hlslPath);
//			}
//		}
//#endif // _DEBUG
//
//		if (now != lastTime) {
//			lastTime = now;
//			resource->Reload();
//
//			// 特定の型に対する追加処理
//			if (std::dynamic_pointer_cast<Shader>(resource)) {
//				UpdateShaderNames();
//			}
//		}
//	}
//#endif // _DEBUG
//}
//
//void ResourceManager::LoadAllShaders()
//{
//	namespace fs = std::filesystem;
//	const std::string shaderDir = "./Data/Shaders/";
//	if (fs::exists(shaderDir) && fs::is_directory(shaderDir)) {
//		for (const auto& entry : fs::recursive_directory_iterator(shaderDir)) {
//			if (entry.is_regular_file()) {
//				const auto& path = entry.path();
//				// シェーダーファイルの拡張子をチェック（.cso）
//				if (path.extension() == ".cso")
//				{
//					std::string pathStr = path.string();
//
//					// ファイル名からシェーダーターゲットを推定
//					std::string stem = path.stem().string(); // ファイル名（拡張子なし）
//					std::string suffix = stem.substr(stem.size() - 2, 2); // ファイル名の末尾2文字を取得
//					std::transform(suffix.begin(), suffix.end(), suffix.begin(), ::tolower); // 小文字に変換
//					std::shared_ptr<Shader> shader;
//					if (suffix == "vs") {
//						// 頂点シェーダーのロード
//						shader = Load<VertexShader>(pathStr);
//					}
//					else if (suffix == "ps") {
//						// ピクセルシェーダーのロード
//						shader = Load<PixelShader>(pathStr);
//					}
//					else if (suffix == "gs") {
//						// ジオメトリシェーダーのロード
//						shader = Load<GeometryShader>(pathStr);
//					}
//					else if (suffix == "cs") {
//						// コンピュートシェーダーのロード
//						shader = Load<ComputeShader>(pathStr);
//					}
//					else if (suffix == "hs") {
//						// ハルシェーダーのロード
//						shader = Load<HullShader>(pathStr);
//					}
//					else if (suffix == "ds") {
//						// ドメインシェーダーのロード
//						shader = Load<DomainShader>(pathStr);
//					}
//					else {
//						continue; // 不明なタイプはスキップ
//					}
//
//					if (shader) {
//						Console::Log("Loaded shader: " + path.string());
//					}
//					else {
//						Console::LogWarning("Failed to load shader: " + path.string());
//					}
//				}
//			}
//		}
//	}
//	// シェーダーパスのキャッシュを更新
//	UpdateShaderNames();
//}
//
//void ResourceManager::LoadAllTextures()
//{
//	// テクスチャフォルダ内のすべてのテクスチャファイルを読み込み
//	namespace fs = std::filesystem;
//	const std::string textureDir = "./Data/Images/";
//	if (fs::exists(textureDir) && fs::is_directory(textureDir)) {
//		for (const auto& entry : fs::recursive_directory_iterator(textureDir)) {
//			if (entry.is_regular_file()) {
//				const auto& path = entry.path();
//				// テクスチャファイルの拡張子をチェック（例: .png, .jpg, .jpeg, .tga, .dds）
//				std::string ext = path.extension().string();
//				std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower); // 小文字に変換
//				if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".dds") {
//					std::string pathStr = path.string();
//					auto texture = Load<Texture>(pathStr);
//					if (texture) {
//						Console::Log("Loaded texture: " + path.string());
//					}
//					else {
//						Console::LogWarning("Failed to load texture: " + path.string());
//					}
//				}
//			}
//		}
//	}
//}
//
//void ResourceManager::UpdateShaderNames()
//{
//#ifdef _DEBUG
//	// 既存のキャッシュをクリア
//	shaderPaths.clear();
//	// シェーダーリソースを走査して名前を更新
//	for (const auto& [path, resource] : _resources) {
//		if (std::dynamic_pointer_cast<Shader>(resource)) {
//			std::filesystem::path fsPath(path);
//
//			// csoファイルだったら、hlslに変換して登録
//			if (fsPath.extension() == ".cso")
//			{
//				// ./Data/Shaders/以下のパスを./Shader/以下に変換して探す
//				std::filesystem::path fileName = fsPath.filename();
//				fsPath = std::filesystem::path(shaderDir) / fileName;
//				fsPath.replace_extension(".hlsl");
//				// 存在するならそれを登録
//				if (std::filesystem::exists(fsPath))
//				{
//					shaderPaths.push_back(fsPath.string());
//					continue; // すでに追加したので次へ
//				}
//			}
//			// フルパスを追加
//			shaderPaths.push_back(path);
//		}
//	}
//#endif // _DEBUG
//
//}