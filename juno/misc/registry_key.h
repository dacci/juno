// Copyright (c) 2013 dacci.org

#ifndef JUNO_MISC_REGISTRY_KEY_H_
#define JUNO_MISC_REGISTRY_KEY_H_

#include <windows.h>

#include <string>
#include <utility>

class RegistryKey {
 public:
  RegistryKey();
  explicit RegistryKey(HKEY key);
  RegistryKey(RegistryKey&& other);

  virtual ~RegistryKey();

  void Attach(HKEY key);
  HKEY Detach();

  bool Create(HKEY parent, const char* name);
  bool Create(HKEY parent, const std::string& name);
  bool Create(HKEY parent, const wchar_t* name);
  bool Create(HKEY parent, const std::wstring& name);

  bool Open(HKEY parent, const char* name);
  bool Open(HKEY parent, const std::string& name);
  bool Open(HKEY parent, const wchar_t* name);
  bool Open(HKEY parent, const std::wstring& name);

  void Close();

  bool QueryString(const char* name, std::string* value);
  std::string QueryString(const char* name);
  std::string QueryString(const std::string& name);

  bool QueryString(const wchar_t* name, std::wstring* value);
  std::wstring QueryString(const wchar_t* name);
  std::wstring QueryString(const std::wstring& name);

  bool SetString(const char* name, const std::string& value);
  bool SetString(const std::string& name, const std::string& value);
  bool SetString(const wchar_t* name, const std::wstring& value);
  bool SetString(const std::wstring& name, const std::wstring& value);

  bool QueryInteger(const char* name, int* value);
  bool QueryInteger(const std::string& name, int* value);
  int QueryInteger(const char* name);
  int QueryInteger(const std::string& name);

  bool QueryInteger(const wchar_t* name, int* value);
  bool QueryInteger(const std::wstring& name, int* value);
  int QueryInteger(const wchar_t* name);
  int QueryInteger(const std::wstring& name);

  bool EnumerateKey(int index, std::string* name);
  bool EnumerateKey(int index, std::wstring* name);

  RegistryKey& operator=(RegistryKey&& other);

  operator bool() const {
    return key_ != NULL;
  }

  operator HKEY() const {
    return key_;
  }

 private:
  HKEY key_;

  RegistryKey(const RegistryKey&);
  RegistryKey& operator=(const RegistryKey&);
};

#endif  // JUNO_MISC_REGISTRY_KEY_H_
