// Copyright (c) 2013 dacci.org

#ifndef JUNO_MISC_STRING_UTIL_H_
#define JUNO_MISC_STRING_UTIL_H_

#include <stdlib.h>
#include <string.h>

#include <string>

inline int _stricmp(const std::string& a, const char* b) {
  return _stricmp(a.c_str(), b);
}

inline int _stricmp(const char* a, const std::string& b) {
  return _stricmp(a, b.c_str());
}

inline int _stricmp(const std::string& a, const std::string& b) {
  return _stricmp(a.c_str(), b.c_str());
}

inline __int64 _strtoi64(const std::string& string, char** end_pointer,
                         int radix) {
  return _strtoi64(string.c_str(), end_pointer, radix);
}

inline std::wstring to_wstring(const std::string& string) {
  return std::wstring(string.begin(), string.end());
}

namespace juno {
namespace misc {

std::string GenerateGUID();

}  // namespace misc
}  // namespace juno

#endif  // JUNO_MISC_STRING_UTIL_H_
