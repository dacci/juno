// Copyright (c) 2016 dacci.org

#ifndef JUNO_MISC_SCHANNEL_SCHANNEL_CREDENTIAL_H_
#define JUNO_MISC_SCHANNEL_SCHANNEL_CREDENTIAL_H_

#if !defined(SECURITY_WIN32) && !defined(SECURITY_KERNEL)
#define SECURITY_WIN32
#endif

#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "secur32.lib")

#include <windows.h>
#include <schnlsp.h>
#include <security.h>

#include <vector>

class SchannelCredential {
 public:
  SchannelCredential() : auth_data_(), expiry_() {
    SecInvalidateHandle(&handle_);

    auth_data_.dwVersion = SCHANNEL_CRED_VERSION;
  }

  virtual ~SchannelCredential() {
    for (auto& certificate : certificates_)
      CertFreeCertificateContext(certificate);

    if (SecIsValidHandle(&handle_)) {
      FreeCredentialsHandle(&handle_);
      SecInvalidateHandle(&handle_);
    }
  }

  HRESULT AcquireHandle(ULONG usage) {
    if (SecIsValidHandle(&handle_))
      return SEC_E_INTERNAL_ERROR;

    return AcquireCredentialsHandle(nullptr, UNISP_NAME, usage, nullptr,
                                    &auth_data_, nullptr, nullptr, &handle_,
                                    &expiry_);
  }

  void AddCertificate(const CERT_CONTEXT* certificate) {
    certificates_.push_back(CertDuplicateCertificateContext(certificate));
    auth_data_.cCreds = static_cast<DWORD>(certificates_.size());
    auth_data_.paCred = certificates_.data();
  }

  void AddSupportedAlgorithm(ALG_ID algorithm) {
    algorithms_.push_back(algorithm);
    auth_data_.cSupportedAlgs = static_cast<DWORD>(algorithms_.size());
    auth_data_.palgSupportedAlgs = algorithms_.data();
  }

  void SetEnabledProtocols(DWORD protocols) {
    auth_data_.grbitEnabledProtocols = protocols;
  }

  void SetMinimumCipherStrength(DWORD strength) {
    auth_data_.dwMinimumCipherStrength = strength;
  }

  void SetMaximumCipherStrength(DWORD strength) {
    auth_data_.dwMaximumCipherStrength = strength;
  }

  void SetFlags(DWORD flags) {
    auth_data_.dwFlags = flags;
  }

 private:
  friend class SchannelContext;

  SCHANNEL_CRED auth_data_;
  CredHandle handle_;
  TimeStamp expiry_;

  std::vector<const CERT_CONTEXT*> certificates_;
  std::vector<ALG_ID> algorithms_;

  SchannelCredential(const SchannelCredential&) = delete;
  SchannelCredential& operator=(const SchannelCredential&) = delete;
};

#endif  // JUNO_MISC_SCHANNEL_SCHANNEL_CREDENTIAL_H_
