#pragma once
#include <filesystem>
#include <mutex>
#include <queue>
#include <source_location>
#include "LoggerConfig.h"

class Logger
{
public:
	constexpr static const char* OutputPath = ".\\Data\\Log";

	enum class LogCategory : uint8_t
	{
		Gameplay,
		Physics,
		UI,
		System,
	};
#ifdef ENABLE_LOGGER

public:
	Logger();
	~Logger();

	static void Initialize();

	static void Log(const char* message);
	static void Log(const char8_t* message);
	static void Log(const std::string& message);

	static void Log(LogCategory category, const char* message);
	static void Log(LogCategory category, const char8_t* message);
    static void Log(LogCategory category, const std::string& message);

    static void Warning(const char* message);
	static void Warning(const char8_t* message);

	static void Warning(LogCategory category, const char* message);
	static void Warning(LogCategory category, const char8_t* message);

    static void Error(const char* message,std::source_location location = std::source_location::current());
	static void Error(const char8_t* message,std::source_location location = std::source_location::current());

    static void Error(LogCategory category, const char* message,std::source_location location = std::source_location::current());
	static void Error(LogCategory category, const char8_t* message,std::source_location location = std::source_location::current());
	static void Error(const std::string& message);

    //static void Log(const wchar_t* message);

	static void DrawImGui();

private:
	static Logger& Instance() { static Logger instance; return instance; }

	static void LogThreadFunc();
private:
	std::filesystem::path logfilePath;

	std::string log;

	struct LogItem
	{
		time_t time;
		LogCategory category;
		std::string message;
        std::string timeString;

		std::string drawLine;   // 描画用キャッシュ
		uint8_t severity;       // 0=Normal 1=Warning 2=Error
	};
	std::queue<LogItem> logQueue;
	std::mutex mtx;

	std::thread logThread_;
	inline static bool logThreadLoop = true;

    std::vector<LogItem> logItems; // Logger メンバ
	inline static bool autoScroll = true;

	bool requestAutoScroll = false;
public:
	inline static bool showGameplay = true;
	inline static bool showPhysics = true;
	inline static bool showUI = true;
	inline static bool showSystem = true;
#else
	static void Initialize() {}
	template<class... T> static void Log(T&&...) {}
	template<class... T> static void Warning(T&&...) {}
	template<class... T> static void Error(T&&...) {}
	static void DrawImGui() {}
#endif
};
