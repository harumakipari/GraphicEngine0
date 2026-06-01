#include "pch.h"
#include "Logger.h"

#ifdef USE_IMGUI
#define IMGUI_ENABLE_DOCKING
#include "imgui.h"
#endif


#include <filesystem>
#include <fstream>
#include <chrono>
#include <format>


static constexpr const char* fmt = "%Y-%m-%d__%H-%M-%S";

using namespace std::chrono;
#ifdef ENABLE_LOGGER

const char* CategoryToString(Logger::LogCategory c)
{
    switch (c)
    {
    case Logger::LogCategory::Gameplay: return "[Gameplay]";
    case Logger::LogCategory::Physics:  return "[Physics ]";
    case Logger::LogCategory::UI:       return "[UI      ]";
    case Logger::LogCategory::System:   return "[System  ]";
    }
    return "[Unknown ]";
}

bool IsCategoryVisible(Logger::LogCategory category)
{
    switch (category)
    {
    case Logger::LogCategory::Gameplay: return Logger::showGameplay;
    case Logger::LogCategory::Physics:  return Logger::showPhysics;
    case Logger::LogCategory::UI:       return Logger::showUI;
    case Logger::LogCategory::System:   return Logger::showSystem;
    }
    return true;
}

Logger::Logger() {
    logThread_ = std::thread(&Logger::LogThreadFunc);
}

Logger::~Logger() {
    logThreadLoop = false;
    if (logThread_.joinable()) {
        logThread_.join();
    }
}

void Logger::Initialize() {

    auto& instance = Instance();

    //現在時刻を取得
    auto now = std::time(nullptr);
    //auto* localTime = std::localtime(&now);
    std::tm localTime{};
    localtime_s(&localTime, &now);

    char timeStr[80];
    //std::strftime(timeStr, sizeof(timeStr), fmt, localTime);
    std::strftime(timeStr, sizeof(timeStr), fmt, &localTime);
    std::string filename;
    filename = "log_";
    filename += timeStr;
    filename += ".txt";

    instance.logfilePath = OutputPath;
    instance.logfilePath /= filename;

    //絶対パス化
    instance.logfilePath = std::filesystem::absolute(instance.logfilePath);

    //ディレクトリがなければ作成
    std::filesystem::create_directories(instance.logfilePath.parent_path());

    instance.logItems.clear();
    instance.log.clear();
    //instance.log.reserve(SIZE_MAX);//無限にログを貯められるようにする 大きすぎるので小さくする
    instance.log.reserve(INT_MAX);//無限にログを貯められるようにする

}

void Logger::Log(LogCategory category, const char* message)
{
    std::lock_guard lock(Instance().mtx);
    Instance().logQueue.emplace(std::time(nullptr), category, message);
}

void Logger::Log(LogCategory category, const char8_t* message)
{
    Log(category, reinterpret_cast<const char*>(message));
}

void Logger::Log(LogCategory category, const  std::string& message)
{
    Log(category, message.c_str());
}

void Logger::Log(const char* message)
{
    Log(LogCategory::Gameplay, message);
}

void Logger::Log(const char8_t* message)
{
    Log(LogCategory::Gameplay, reinterpret_cast<const char*>(message));
}

void Logger::Log(const std::string& message)
{
    Log(LogCategory::Gameplay, message.c_str());
}

void Logger::Warning(LogCategory category, const char* message)
{
    std::lock_guard lock(Instance().mtx);
    Instance().logQueue.emplace(std::time(nullptr), category, std::format("[WARNING] {}", message));
}

void Logger::Warning(const char* message)
{
    Warning(LogCategory::Gameplay, message);
}

void Logger::Warning(const char8_t* message)
{
    Warning(reinterpret_cast<const char*>(message));
}

void Logger::Warning(LogCategory category, const char8_t* message)
{
    Warning(category, reinterpret_cast<const char*>(message));
}

void Logger::Error(LogCategory category, const char* message, std::source_location location)
{
    std::lock_guard lock(Instance().mtx);
    Instance().logQueue.emplace(std::time(nullptr), category, std::format("[ERROR]\n\n\n ErrorLocation: \n\n\n\t File : {}\n\n\n\t Function : {}\n\n\n\t Line : {}\n\n\n Message :\n\n\n\t {}", location.file_name(), location.function_name(), location.line(), message));
}

void Logger::Error(const char8_t* message, std::source_location location)
{
    Error(reinterpret_cast<const char*>(message), location);
}

void Logger::Error(const char* message, std::source_location location)
{
    Error(LogCategory::Gameplay, message, location);
}

void Logger::Error(LogCategory category, const char8_t* message, std::source_location location)
{
    Error(category, reinterpret_cast<const char*>(message), location);
}

void Logger::Error(const std::string& message)
{
    Error(LogCategory::Gameplay, message.c_str());
}


void Logger::DrawImGui()
{
#ifdef USE_IMGUI
    ImGui::Begin(U8("ログ情報"));


    ImGui::Checkbox("Gameplay", &showGameplay);
    ImGui::SameLine();
    ImGui::Checkbox("Physics", &showPhysics);
    ImGui::SameLine();
    ImGui::Checkbox("UI", &showUI);
    ImGui::SameLine();
    ImGui::Checkbox("System", &showSystem);

    ImGui::SameLine();
    if (ImGui::Button("Clear"))
    {
        auto& instance = Instance();
        instance.logItems.clear();
        instance.log.clear();
    }

    ImGui::SameLine();
    ImGui::Checkbox("AutoScroll", &autoScroll);
    ImGui::Separator();

    auto& instance = Instance();

    ImGui::BeginChild(
        "LoggerScroll",
        ImVec2(0, 0),
        true,
        ImGuiWindowFlags_HorizontalScrollbar
    );

    static std::vector<const LogItem*> visibleItems;
    visibleItems.clear();

    for (const auto& item : instance.logItems)
    {
        if (IsCategoryVisible(item.category))
            visibleItems.push_back(&item);
    }

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(visibleItems.size()));

    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
        {
            const auto& item = *visibleItems[i];

            if (item.severity == 2)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.2f, 0.2f, 1));
            else if (item.severity == 1)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.8f, 0.2f, 1));

            ImGui::TextUnformatted(item.drawLine.c_str());

            if (item.severity != 0)
                ImGui::PopStyleColor();
        }
    }
    if (autoScroll && instance.requestAutoScroll)
    {
        ImGui::SetScrollHereY(1.0f);
        instance.requestAutoScroll = false;
    }
    ImGui::EndChild();

    ImGui::End();
#endif
}


void Logger::LogThreadFunc() {
    while (logThreadLoop)
    {
        if (Instance().logQueue.empty())continue;

        auto& instance = Instance();
        std::lock_guard lock(instance.mtx);
        std::ofstream ofs(instance.logfilePath, std::ios::out | std::ios::app);
        if (!ofs.is_open())return;

        time_t currentTime = 0;
        std::string timeStr;

        while (!instance.logQueue.empty())
        {
            LogItem item = instance.logQueue.front();
            instance.logQueue.pop();

            if (currentTime != item.time)
            {
                currentTime = item.time;
                std::tm localTime{};
                localtime_s(&localTime, &currentTime);
                char buf[80];
                std::strftime(buf, sizeof(buf), fmt, &localTime);
                timeStr = buf;
            }

            item.timeString = timeStr;

            if (item.message.find("[ERROR]") != std::string::npos)
                item.severity = 2;
            else if (item.message.find("[WARNING]") != std::string::npos)
                item.severity = 1;
            else
                item.severity = 0;

            // drawLine をここで作る（超重要）
            item.drawLine = std::format(
                "{} {} : {}",
                CategoryToString(item.category),
                item.timeString,
                item.message
            );

            ofs << item.drawLine << '\n';
            instance.logItems.push_back(std::move(item));
            instance.requestAutoScroll = true;
        }
        ofs.close();
    }
}

#endif