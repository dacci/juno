// Copyright (c) 2013 dacci.org

#ifndef JUNO_MISC_SCHANNEL_SECURITY_BUFFER_H_
#define JUNO_MISC_SCHANNEL_SECURITY_BUFFER_H_

#include <windows.h>
#include <security.h>

#include <vector>

struct SecurityBuffer : SecBuffer {
  SecurityBuffer() : SecBuffer() {
  }

  SecurityBuffer(ULONG type, ULONG size, void* buffer) {
    cbBuffer = size;
    BufferType = type;
    pvBuffer = buffer;
  }

  SECURITY_STATUS Free() {
    SECURITY_STATUS status = ::FreeContextBuffer(pvBuffer);
    if (SUCCEEDED(status)) {
      cbBuffer = 0;
      pvBuffer = nullptr;
    }

    return status;
  }
};

class SecurityBufferBundle : public SecBufferDesc {
 public:
  SecurityBufferBundle() : SecBufferDesc() {
    ulVersion = SECBUFFER_VERSION;
  }

  SecurityBuffer* AddBuffer(ULONG type, ULONG size, void* buffer) {
    buffers_.push_back(SecurityBuffer(type, size, buffer));
    cBuffers = buffers_.size();
    pBuffers = buffers_.data();
    return &buffers_.back();
  }

  SecurityBuffer* AddBuffer(ULONG type) {
    return AddBuffer(type, 0, nullptr);
  }

  void ClearBuffers() {
    buffers_.clear();
  }

  void FreeBuffers() {
    for (auto i = buffers_.begin(), l = buffers_.end(); i != l; ++i)
      i->Free();
  }

  SecBuffer& at(size_t index) {
    return buffers_[index];
  }

  SecBuffer& operator[](size_t index) {
    return buffers_[index];
  }

 private:
  std::vector<SecurityBuffer> buffers_;
};

#endif  // JUNO_MISC_SCHANNEL_SECURITY_BUFFER_H_
