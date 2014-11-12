// Copyright (c) 2013 dacci.org

#ifndef JUNO_MISC_SECURITY_SCHANNEL_CONTEXT_H_
#define JUNO_MISC_SECURITY_SCHANNEL_CONTEXT_H_

#include <string.h>
#include <tchar.h>

#include <string>

#include "misc/security/schannel_credential.h"

class SchannelContext {
 public:
  explicit SchannelContext(SchannelCredential* credential)
      : credential_(credential),
        attributes_(),
        expiry_() {
    SecInvalidateHandle(&handle_);
  }

  virtual ~SchannelContext() {
    if (SecIsValidHandle(&handle_)) {
      ::DeleteSecurityContext(&handle_);
      SecInvalidateHandle(&handle_);
    }
  }

  HRESULT InitializeContext(ULONG request, SecBufferDesc* input,
                            SecBufferDesc* output) {
    PCtxtHandle in_handle = nullptr, out_handle = nullptr;
    if (SecIsValidHandle(&handle_))
      in_handle = &handle_;
    else
      out_handle = &handle_;

    char* target_name = nullptr;
    if (!target_name_.empty())
      target_name = &target_name_[0];

    return ::InitializeSecurityContextA(&credential_->handle_,
                                        in_handle,
                                        target_name,
                                        request,
                                        0,     // reserved
                                        0,     // unused
                                        input,
                                        0,     // reserved
                                        out_handle,
                                        output,
                                        &attributes_,
                                        &expiry_);
  }

  HRESULT AcceptContext(ULONG request, SecBufferDesc* input,
                        SecBufferDesc* output) {
    PCtxtHandle in_handle = nullptr, out_handle = nullptr;
    if (SecIsValidHandle(&handle_))
      in_handle = &handle_;
    else
      out_handle = &handle_;

    return ::AcceptSecurityContext(&credential_->handle_,
                                   in_handle,
                                   input,
                                   request,
                                   0,         // unused
                                   out_handle,
                                   output,
                                   &attributes_,
                                   &expiry_);
  }

  HRESULT ApplyControlToken(ULONG token) {
    SecBuffer buffer = { sizeof(token), SECBUFFER_TOKEN, &token };
    SecBufferDesc buffers = { SECBUFFER_VERSION, 1, &buffer };

    return ::ApplyControlToken(&handle_, &buffers);
  }

  HRESULT QueryAttributes(ULONG attribute, void* buffer) {
    return ::QueryContextAttributesA(&handle_, attribute, buffer);
  }

  HRESULT EncryptMessage(ULONG qop, SecBufferDesc* message) {
    if (!SecIsValidHandle(&handle_))
      return SEC_E_INVALID_HANDLE;

    return ::EncryptMessage(&handle_, qop, message, 0);
  }

  HRESULT DecryptMessage(SecBufferDesc* message) {
    if (!SecIsValidHandle(&handle_))
      return SEC_E_INVALID_HANDLE;

    return ::DecryptMessage(&handle_, message, 0, nullptr);
  }

  const std::string& target_name() const {
    return target_name_;
  }

  void set_target_name(const std::string& target_name) {
    target_name_ = target_name;
  }

  ULONG attributes() const {
    return attributes_;
  }

  const TimeStamp& expiry() const {
    return expiry_;
  }

  bool IsValid() const {
    return SecIsValidHandle(&handle_);
  }

  operator bool() const {
    return IsValid();
  }

 private:
  SchannelCredential* const credential_;
  std::string target_name_;
  CtxtHandle handle_;
  ULONG attributes_;
  TimeStamp expiry_;

  SchannelContext(const SchannelContext&);
  SchannelContext& operator=(const SchannelContext&);
};

#endif  // JUNO_MISC_SECURITY_SCHANNEL_CONTEXT_H_
