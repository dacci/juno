// Copyright (c) 2013 dacci.org

#ifndef JUNO_MISC_REGISTRY_KEY_INL_H_
#define JUNO_MISC_REGISTRY_KEY_INL_H_

#include "misc/registry_key.h"

#include <string>

namespace juno {
namespace misc {

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

inline std::string RegistryKey::QueryString(const char* name) const {
  std::string result;
  QueryString(name, &result);
  return result;
}

inline bool RegistryKey::QueryString(const std::string& name,
                                     std::string* value) const {
  return QueryString(name.c_str(), value);
}

inline std::string RegistryKey::QueryString(const std::string& name) const {
  return QueryString(name.c_str());
}

inline bool RegistryKey::QueryString(const std::wstring& name,
                                     std::wstring* value) const {
  return QueryString(name.c_str(), value);
}

inline std::wstring RegistryKey::QueryString(const wchar_t* name) const {
  std::wstring result;
  QueryString(name, &result);
  return result;
}

inline std::wstring RegistryKey::QueryString(const std::wstring& name) const {
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

inline bool RegistryKey::QueryInteger(const std::string& name,
                                      int* value) const {
  return QueryInteger(name.c_str(), value);
}

inline int RegistryKey::QueryInteger(const char* name) const {
  auto value = 0;
  QueryInteger(name, &value);
  return value;
}

inline int RegistryKey::QueryInteger(const std::string& name) const {
  return QueryInteger(name.c_str());
}

inline bool RegistryKey::QueryInteger(const std::wstring& name,
                                      int* value) const {
  return QueryInteger(name.c_str(), value);
}

inline int RegistryKey::QueryInteger(const wchar_t* name) const {
  auto value = 0;
  QueryInteger(name, &value);
  return value;
}

inline int RegistryKey::QueryInteger(const std::wstring& name) const {
  return QueryInteger(name.c_str());
}

inline bool RegistryKey::SetInteger(const std::string& name, int value) {
  return SetInteger(name.c_str(), value);
}

inline bool RegistryKey::SetInteger(const std::wstring& name, int value) {
  return SetInteger(name.c_str(), value);
}

inline bool RegistryKey::QueryBinary(const std::string& name, void* data,
                                     int* length) const {
  return QueryBinary(name.c_str(), data, length);
}

inline bool RegistryKey::QueryBinary(const std::wstring& name, void* data,
                                     int* length) const {
  return QueryBinary(name.c_str(), data, length);
}

inline bool RegistryKey::SetBinary(const std::string& name, const void* data,
                                   int length) {
  return SetBinary(name.c_str(), data, length);
}

inline bool RegistryKey::SetBinary(const std::wstring& name, const void* data,
                                   int length) {
  return SetBinary(name.c_str(), data, length);
}

inline bool RegistryKey::DeleteKey(const std::string& sub_key) {
  return DeleteKey(sub_key.c_str());
}

inline bool RegistryKey::DeleteKey(const std::wstring& sub_key) {
  return DeleteKey(sub_key.c_str());
}

}  // namespace misc
}  // namespace juno

#endif  // JUNO_MISC_REGISTRY_KEY_INL_H_
