#pragma once

#include <windows.h>

#include "imgui.h"

inline LPWSTR hr_trace(HRESULT hr)
{
	LPWSTR msg{ 0 };
	FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&msg), 0, NULL);
	return msg;
}

#define U8(x) reinterpret_cast<const char*>(u8##x)

inline bool CheckboxInt(const char* label, int* v)
{
#ifdef USE_IMGUI
    bool b = (*v != 0);
    if (ImGui::Checkbox(label, &b))
    {
        *v = b ? 1 : 0;
        return true;
    }
    return false;
#endif
}

inline std::string WStringToUTF8(const std::wstring& wStr)
{
    if (wStr.empty()) return {};

    int size = WideCharToMultiByte(
        CP_UTF8, 0,
        wStr.data(), static_cast<int>(wStr.size()),
        nullptr, 0,
        nullptr, nullptr);

    std::string result(size, 0);

    WideCharToMultiByte(
        CP_UTF8, 0,
        wStr.data(), static_cast<int>(wStr.size()),
        result.data(), size,
        nullptr, nullptr);

    return result;
}