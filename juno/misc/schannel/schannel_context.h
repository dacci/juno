// Copyright (c) 2013 dacci.org

#ifndef JUNO_MISC_SCHANNEL_SCHANNEL_CONTEXT_H_
#define JUNO_MISC_SCHANNEL_SCHANNEL_CONTEXT_H_

#include <string.h>
#include <tchar.h>

#include <string>

#include "misc/schannel/schannel_credential.h"

class SchannelContext {
 public:
  SchannelContext(SchannelCredential* credential,
                  const std::string& target_name)
      : credential_(credential),
        target_name_(target_name),
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

  SECURITY_STATUS InitializeContext(ULONG request, SecBufferDesc* input,
                                    SecBufferDesc* output) {
    PCtxtHandle in_handle = NULL, out_handle = NULL;
    if (SecIsValidHandle(&handle_))
      in_handle = &handle_;
    else
      out_handle = &handle_;

    return ::InitializeSecurityContextA(&credential_->handle_,
                                        in_handle,
                                        &target_name_[0],
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

  SECURITY_STATUS AcceptContext(ULONG request, SecBufferDesc* input,
                                SecBufferDesc* output) {
    PCtxtHandle in_handle = NULL, out_handle = NULL;
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

  SECURITY_STATUS ApplyControlToken(ULONG token) {
    SecBuffer buffer = { sizeof(token), SECBUFFER_TOKEN, &token };
    SecBufferDesc buffers = { SECBUFFER_VERSION, 1, &buffer };

    return ::ApplyControlToken(&handle_, &buffers);
  }

  SECURITY_STATUS QueryAttributes(ULONG attribute, void* buffer) {
    return ::QueryContextAttributesA(&handle_, attribute, buffer);
  }

  SECURITY_STATUS EncryptMessage(ULONG qop, SecBufferDesc* message) {
    if (!SecIsValidHandle(&handle_))
      return SEC_E_INVALID_HANDLE;

    return ::EncryptMessage(&handle_, qop, message, 0);
  }

  SECURITY_STATUS DecryptMessage(SecBufferDesc* message) {
    if (!SecIsValidHandle(&handle_))
      return SEC_E_INVALID_HANDLE;

    return ::DecryptMessage(&handle_, message, 0, NULL);
  }

  ULONG attributes() const {
    return attributes_;
  }

  const TimeStamp& expiry() const {
    return expiry_;
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

#endif  // JUNO_MISC_SCHANNEL_SCHANNEL_CONTEXT_H_
