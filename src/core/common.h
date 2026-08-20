#pragma once
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
#include <string>
#include <vector>

namespace sl {
using std::string;
using std::wstring;
using std::vector;

inline wstring a2w(const string& s) {
    if (s.empty()) return wstring();
#ifdef _WIN32
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    wstring w((size_t)n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
#else
    wstring w;
    for (unsigned char c : s) w.push_back((wchar_t)c);
#endif
    return w;
}
inline string w2a(const wstring& w) {
    if (w.empty()) return string();
#ifdef _WIN32
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    string s((size_t)n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
#else
    string s;
    for (wchar_t c : w) s.push_back((char)c);
#endif
    return s;
}
inline string ws2s(const wstring& w) { return w2a(w); }
inline wstring s2ws(const string& s) { return a2w(s); }

// Обрезка пробельных символов по краям (UTF-8 безопасно).
inline string trim(const string& s) {
    size_t b = 0, e = s.size();
    while (b < e && (unsigned char)s[b] <= ' ') b++;
    while (e > b && (unsigned char)s[e - 1] <= ' ') e--;
    return s.substr(b, e - b);
}

} // namespace sl
