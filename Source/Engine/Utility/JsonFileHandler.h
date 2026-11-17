#pragma once
#include "json.hpp"
#include <fstream>
#include <string>

using json = nlohmann::json;

class JsonFileHandler
{
public:
	static void SaveJsonToFile(const json& j, const std::string& filePath)
    {
		std::ofstream file(filePath);
		if (file) {
			file << j.dump(4);
		}
	}

	static bool LoadJsonFromFile(json& j, const std::string& filePath)
    {
		//ファイルが存在しなかったら、新規作成
		if (!std::filesystem::exists(std::filesystem::path(filePath))) {
			std::ofstream ofs(filePath);
			if (ofs) {
				j = json::object();  // 空のオブジェクト {} で初期化
				ofs << j.dump(4);    // インデント付きで書き出し
			}
		}

		std::ifstream ifs(filePath);
		if (!ifs) return false;

		ifs >> j;
		return true;
	}
};