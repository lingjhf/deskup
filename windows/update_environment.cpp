#include "update_environment.h"
#include <winver.h>
#include <chrono>
#include <cwchar>
#include <vector>

namespace deskup {
DWORD ReadPreference(const std::wstring& key, const wchar_t* name) {
  DWORD value = 0;
  DWORD size = sizeof(value);
  RegGetValueW(HKEY_CURRENT_USER, key.c_str(), name, RRF_RT_REG_DWORD,
               nullptr, &value, &size);
  return value;
}
bool WritePreference(const std::wstring& path, const wchar_t* name, DWORD value) {
  HKEY key = nullptr;
  if (RegCreateKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, nullptr, 0,
                      KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
    return false;
  }
  const auto result = RegSetValueExW(key, name, 0, REG_DWORD,
      reinterpret_cast<const BYTE*>(&value), sizeof(value));
  RegCloseKey(key);
  return result == ERROR_SUCCESS;
}
DWORD Now() {
  return static_cast<DWORD>(std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch()).count());
}
std::pair<std::string, std::string> ExecutableVersion() {
  wchar_t executable[32768]{};
  if (!GetModuleFileNameW(nullptr, executable, 32768)) return {};
  DWORD unused = 0;
  const auto size = GetFileVersionInfoSizeW(executable, &unused);
  if (!size) return {};
  std::vector<BYTE> data(size);
  if (!GetFileVersionInfoW(executable, 0, size, data.data())) return {};
  struct Translation { WORD language; WORD code_page; };
  Translation* translation = nullptr;
  UINT translation_size = 0;
  if (VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation",
      reinterpret_cast<void**>(&translation), &translation_size) &&
      translation_size >= sizeof(Translation)) {
    wchar_t key[64]{};
    swprintf_s(key, 64, L"\\StringFileInfo\\%04x%04x\\ProductVersion",
        static_cast<unsigned int>(translation->language),
        static_cast<unsigned int>(translation->code_page));
    wchar_t* text = nullptr;
    UINT text_size = 0;
    if (VerQueryValueW(data.data(), key, reinterpret_cast<void**>(&text), &text_size) &&
        text && text_size > 1) {
      // Flutter's string resource retains build numbers above 65535, whereas
      // VS_FIXEDFILEINFO stores each version component in only sixteen bits.
      const int count = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
      if (count > 1) {
        std::string product(static_cast<size_t>(count), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text, -1, product.data(), count, nullptr, nullptr);
        product.pop_back();
        const auto build = product.find('+');
        if (build != std::string::npos) return {product.substr(0, build), product.substr(build + 1)};
        return {product, ""};
      }
    }
  }
  VS_FIXEDFILEINFO* info = nullptr;
  UINT length = 0;
  if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &length) ||
      length < sizeof(VS_FIXEDFILEINFO)) return {};
  return {std::to_string(HIWORD(info->dwProductVersionMS)) + "." +
      std::to_string(LOWORD(info->dwProductVersionMS)) + "." +
      std::to_string(HIWORD(info->dwProductVersionLS)),
      std::to_string(LOWORD(info->dwProductVersionLS))};
}

}  // namespace deskup
