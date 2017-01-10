// Copyright (c) 2016 dacci.org

#ifndef JUNO_IO_SECURE_CHANNEL_H_
#define JUNO_IO_SECURE_CHANNEL_H_

#include <base/atomic_ref_count.h>
#include <base/synchronization/condition_variable.h>
#include <base/synchronization/lock.h>

#include <map>
#include <memory>
#include <queue>
#include <string>

#include "io/channel.h"
#include "misc/schannel/schannel_context.h"

namespace juno {
namespace io {

class SecureChannel : public Channel, private Channel::Listener {
 public:
  SecureChannel(misc::schannel::SchannelCredential* credential,
                const std::shared_ptr<Channel>& channel, bool inbound);
  virtual ~SecureChannel();

  void Close() override;
  HRESULT ReadAsync(void* buffer, int length,
                    Channel::Listener* listener) override;
  HRESULT WriteAsync(const void* buffer, int length,
                     Channel::Listener* listener) override;

  misc::schannel::SchannelContext* context() {
    return &context_;
  }

  const misc::schannel::SchannelContext* context() const {
    return &context_;
  }

 private:
  struct Request;
  enum class Status;

  static const size_t kBufferSize = 16 * 1024;

  HRESULT CheckMessage() const;
  HRESULT Negotiate();
  HRESULT Decrypt();
  HRESULT Encrypt(const void* input, int length, std::string* output);

  HRESULT EnsureReading();
  HRESULT WriteAsyncImpl(const std::string& message, Request* request);
  HRESULT WriteAsyncImpl(std::unique_ptr<char[]>&& buffer, size_t length,
                         Request* request);

  void OnRead(Channel* channel, HRESULT result, void* raw_buffer,
              int length) override;
  void OnWritten(Channel* channel, HRESULT result, void* raw_buffer,
                 int length) override;

  static void CALLBACK OnRead(PTP_CALLBACK_INSTANCE callback, void* instance,
                              PTP_WORK work);
  void OnRead();

  static void CALLBACK OnWrite(PTP_CALLBACK_INSTANCE callback, void* instance,
                               PTP_WORK work);
  void OnWrite();

  misc::schannel::SchannelContext context_;
  std::shared_ptr<Channel> channel_;
  const bool inbound_;

  base::Lock lock_;
  base::ConditionVariable deletable_;
  base::AtomicRefCount ref_count_;

  Status status_;
  std::string message_;
  SecPkgContext_StreamSizes stream_sizes_;

  std::string decrypted_;
  std::queue<std::unique_ptr<Request>> pending_reads_;
  base::AtomicRefCount reads_;
  PTP_WORK read_work_;

  std::queue<std::unique_ptr<Request>> pending_writes_;
  std::map<void*, std::unique_ptr<Request>> writes_;
  PTP_WORK write_work_;

  SecureChannel(const SecureChannel&) = delete;
  SecureChannel& operator=(const SecureChannel&) = delete;
};

}  // namespace io
}  // namespace juno

#endif  // JUNO_IO_SECURE_CHANNEL_H_
