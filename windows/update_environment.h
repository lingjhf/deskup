#pragma once
#include <windows.h>
#include <string>
#include <utility>

namespace deskup {
DWORD ReadPreference(const std::wstring& key, const wchar_t* name);
bool WritePreference(const std::wstring& key, const wchar_t* name, DWORD value);
DWORD Now();
std::pair<std::string, std::string> ExecutableVersion();
}  // namespace deskup
