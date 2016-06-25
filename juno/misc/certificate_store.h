// Copyright (c) 2016 dacci.org

#ifndef JUNO_MISC_CERTIFICATE_STORE_H_
#define JUNO_MISC_CERTIFICATE_STORE_H_

#pragma comment(lib, "crypt32.lib")

#include <windows.h>
#include <cryptuiapi.h>

class CertificateStore {
 public:
  explicit CertificateStore(const wchar_t* store_name) : store_handle_() {
    store_handle_ = CertOpenSystemStore(NULL, store_name);
  }

  CertificateStore(const char* provider, DWORD encoding, DWORD flags,
                   const void* param)
      : store_handle_() {
    store_handle_ = CertOpenStore(provider, encoding, NULL, flags, param);
  }

  ~CertificateStore() {
    if (IsValid())
      CertCloseStore(store_handle_, 0);
  }

  PCCERT_CONTEXT EnumCertificate(PCCERT_CONTEXT previous) {
    if (!IsValid())
      return nullptr;

    return CertEnumCertificatesInStore(store_handle_, previous);
  }

  PCCERT_CONTEXT FindCertificate(DWORD encoding, DWORD flags, DWORD type,
                                 const void* data, PCCERT_CONTEXT previous) {
    if (!IsValid())
      return nullptr;

    return CertFindCertificateInStore(store_handle_, encoding, flags, type,
                                      data, previous);
  }

  PCCERT_CONTEXT SelectCertificate(HWND parent, const wchar_t* title,
                                   const wchar_t* message, DWORD exclude) {
    if (!IsValid())
      return nullptr;

    return CryptUIDlgSelectCertificateFromStore(store_handle_, parent, title,
                                                message, exclude, 0, nullptr);
  }

  operator HCERTSTORE() const {
    return store_handle_;
  }

  bool IsValid() const {
    return store_handle_ != NULL;
  }

  operator bool() const {
    return IsValid();
  }

 private:
  HCERTSTORE store_handle_;

  CertificateStore(const CertificateStore&) = delete;
  CertificateStore& operator=(const CertificateStore&) = delete;
};

#endif  // JUNO_MISC_CERTIFICATE_STORE_H_
