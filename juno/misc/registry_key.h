// Copyright (c) 2013 dacci.org

#ifndef JUNO_MISC_REGISTRY_KEY_H_
#define JUNO_MISC_REGISTRY_KEY_H_

#include <windows.h>

#include <string>

namespace juno {
namespace misc {

class RegistryKey {
 public:
  RegistryKey();
  explicit RegistryKey(HKEY key);
  RegistryKey(RegistryKey&& other) noexcept;

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

  bool QueryString(const char* name, std::string* value) const;
  bool QueryString(const std::string& name, std::string* value) const;
  std::string QueryString(const char* name) const;
  std::string QueryString(const std::string& name) const;

  bool QueryString(const wchar_t* name, std::wstring* value) const;
  bool QueryString(const std::wstring& name, std::wstring* value) const;
  std::wstring QueryString(const wchar_t* name) const;
  std::wstring QueryString(const std::wstring& name) const;

  bool SetString(const char* name, const std::string& value);
  bool SetString(const std::string& name, const std::string& value);
  bool SetString(const wchar_t* name, const std::wstring& value);
  bool SetString(const std::wstring& name, const std::wstring& value);

  bool QueryInteger(const char* name, int* value) const;
  bool QueryInteger(const std::string& name, int* value) const;
  int QueryInteger(const char* name) const;
  int QueryInteger(const std::string& name) const;

  bool QueryInteger(const wchar_t* name, int* value) const;
  bool QueryInteger(const std::wstring& name, int* value) const;
  int QueryInteger(const wchar_t* name) const;
  int QueryInteger(const std::wstring& name) const;

  bool SetInteger(const char* name, int value);
  bool SetInteger(const std::string& name, int value);
  bool SetInteger(const wchar_t* name, int value);
  bool SetInteger(const std::wstring& name, int value);

  bool QueryBinary(const char* name, void* data, int* length) const;
  bool QueryBinary(const std::string& name, void* data, int* length) const;
  bool QueryBinary(const wchar_t* name, void* data, int* length) const;
  bool QueryBinary(const std::wstring& name, void* data, int* length) const;

  bool SetBinary(const char* name, const void* data, int length);
  bool SetBinary(const std::string& name, const void* data, int length);
  bool SetBinary(const wchar_t* name, const void* data, int length);
  bool SetBinary(const std::wstring& name, const void* data, int length);

  bool EnumerateKey(int index, std::string* name);
  bool EnumerateKey(int index, std::wstring* name);

  bool DeleteKey(const char* sub_key);
  bool DeleteKey(const std::string& sub_key);
  bool DeleteKey(const wchar_t* sub_key);
  bool DeleteKey(const std::wstring& sub_key);

  RegistryKey& operator=(RegistryKey&& other) noexcept;

  operator bool() const {
    return key_ != NULL;
  }

  operator HKEY() const {
    return key_;
  }

 private:
  HKEY key_;

  RegistryKey(const RegistryKey&) = delete;
  RegistryKey& operator=(const RegistryKey&) = delete;
};

}  // namespace misc
}  // namespace juno

#endif  // JUNO_MISC_REGISTRY_KEY_H_
