// Copyright (c) 2013 dacci.org

#ifndef JUNO_MISC_REGISTRY_KEY_INL_H_
#define JUNO_MISC_REGISTRY_KEY_INL_H_

#include "misc/registry_key.h"

inline bool RegistryKey::Create(HKEY parent, const std::string& name) {
  return Create(parent, name.c_str());
}

inline bool RegistryKey::Create(HKEY parent, const std::wstring& name) {
  return Create(parent, name.c_str());
}

inline bool RegistryKey::Open(HKEY parent, const std::string& name) {
  return Open(parent, name.c_str());
}

inline bool RegistryKey::Open(HKEY parent, const std::wstring& name) {
  return Open(parent, name.c_str());
}

inline std::string RegistryKey::QueryString(const char* name) {
  std::string result;
  QueryString(name, &result);
  return result;
}

inline std::string RegistryKey::QueryString(const std::string& name) {
  return QueryString(name.c_str());
}

inline std::wstring RegistryKey::QueryString(const wchar_t* name) {
  std::wstring result;
  QueryString(name, &result);
  return result;
}

inline std::wstring RegistryKey::QueryString(const std::wstring& name) {
  return QueryString(name.c_str());
}

inline bool RegistryKey::SetString(const std::string& name,
                                   const std::string& value) {
  return SetString(name.c_str(), value);
}

inline bool RegistryKey::SetString(const std::wstring& name,
                                   const std::wstring& value) {
  return SetString(name.c_str(), value);
}

inline bool RegistryKey::QueryInteger(const std::string& name, int* value) {
  return QueryInteger(name.c_str(), value);
}

inline int RegistryKey::QueryInteger(const char* name) {
  int value = 0;
  QueryInteger(name, &value);
  return value;
}

inline int RegistryKey::QueryInteger(const std::string& name) {
  return QueryInteger(name.c_str());
}

inline bool RegistryKey::QueryInteger(const std::wstring& name, int* value) {
  return QueryInteger(name.c_str(), value);
}

inline int RegistryKey::QueryInteger(const wchar_t* name) {
  int value = 0;
  QueryInteger(name, &value);
  return value;
}

inline int RegistryKey::QueryInteger(const std::wstring& name) {
  return QueryInteger(name.c_str());
}

#endif  // JUNO_MISC_REGISTRY_KEY_INL_H_
