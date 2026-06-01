//#pragma once
//#include "Resource.h"
//#include <string>
//#include <memory>
//#include <unordered_map>
//#include <filesystem>
//
//class ResourceManager
//{
//public:
//	// 初期化
//	static void Initialize();
//
//	// 終了処理
//	static void Finalize();
//
//	// リソースの監視登録（ホットリロード用）
//	static void Register(const std::string& filePath, std::shared_ptr<Resource> resource) {
//		resource->AddRef();
//		_resources[filePath] = resource;
//		_files[filePath] = std::filesystem::last_write_time(filePath);// 最終更新日時を保存
//		std::filesystem::path fsPath(filePath);
//#ifdef _DEBUG
//		if (fsPath.extension() == ".cso") {
//			// csoファイルだったら、hlslに変換して登録
//			std::filesystem::path hlslPath = shaderDir / fsPath.filename().replace_extension(".hlsl");
//
//			// hlslファイルも監視対象に追加
//			if (std::filesystem::exists(hlslPath))
//			{
//				_resources[hlslPath.string()] = resource;
//				_files[hlslPath.string()] = std::filesystem::last_write_time(hlslPath);
//			}
//		}
//#endif // _DEBUG
//	}
//
//	// リソースのロード（すでにロードされている場合は既存のリソースを返す）
//	template<typename T>
//	static std::shared_ptr<T> Load(const std::string& path)
//	{
//		// すでにロードされているか確認
//		auto it = _resources.find(path);
//		if (it != _resources.end()) {
//			return std::dynamic_pointer_cast<T>(it->second); // 既存のリソースを返す
//		}
//		// 新しいリソースを作成
//		std::shared_ptr<Resource> resource = std::make_shared<T>();
//		if (resource->LoadFromFile(path)) {
//			// ロード成功したら管理に追加
//			Register(path, resource);
//			return std::dynamic_pointer_cast<T>(resource);
//		}
//		return nullptr; // ロード失敗
//	}
//	// リソースの取得（ロードされていない場合は nullptr を返す）
//	static std::shared_ptr<Resource> Get(const std::string& path);
//
//	// 指定した型でリソースを取得（ロードされていない場合は nullptr を返す）
//	template<typename T>
//	static std::shared_ptr<T> GetAs(const std::string& path) {
//		return std::dynamic_pointer_cast<T>(Get(path));
//	}
//
//	// 指定した型のすべてのリソースを取得
//	template<typename T>
//	static std::vector<std::shared_ptr<T>> GetResourcesOfType() {
//		std::vector<std::shared_ptr<T>> result;
//		for (const auto& [path, resource] : _resources) {
//			if (auto casted = std::dynamic_pointer_cast<T>(resource)) {
//				result.push_back(casted);
//			}
//		}
//		return result;
//	}
//
//	// ロードされているシェーダーパスの一覧を取得
//	static std::vector<std::string> GetShaderPaths();
//
//	// リソースのリロード
//	static void Reload(const std::string& path);
//
//	// リソースのアンロード
//	static void Unload(const std::string& path);
//
//	// 全リソースのアンロード
//	static void UnloadAll();
//
//	// ホットリロード用の更新処理
//	static void Update();
//
//private:
//	// ホットリロード用の更新処理
//	static void UpdateHotReload();
//
//	// シェーダーフォルダ内のすべてのシェーダーファイルを読み込み
//	static void LoadAllShaders();
//
//	// テクスチャフォルダ内のすべてのテクスチャファイルを読み込み
//	static void LoadAllTextures();
//
//	// シェーダー名のキャッシュを更新
//	static void UpdateShaderNames();
//private:
//	// ファイルの最終更新日時を管理（ホットリロード用）
//	static inline std::unordered_map<std::string, std::filesystem::file_time_type> _files;
//
//	// パスをキーにしてリソースを管理
//	static inline std::unordered_map<std::string, std::shared_ptr<Resource>> _resources;
//
//	//static constexpr inline const char* watchPrefix = "./Data/Shaders/"; // 監視対象のファイルパスの接頭辞
//
//	static constexpr inline const char* shaderDir = "./Shader/"; // HLSL ファイルの配置先ディレクトリ
//
//	static inline std::vector<std::string> shaderPaths; // ロードされているシェーダーパスのキャッシュ
//};