// Copyright (c) 2017 dacci.org

#ifndef JUNO_IO_NAMED_PIPE_CHANNEL_H_
#define JUNO_IO_NAMED_PIPE_CHANNEL_H_

#include <base/strings/string_piece.h>
#include <base/synchronization/lock.h>

#include <memory>
#include <queue>

#include "io/channel.h"

namespace juno {
namespace io {

class NamedPipeChannel : public Channel {
 public:
  class __declspec(novtable) Listener {
   public:
    virtual ~Listener() {}

    virtual void OnConnected(NamedPipeChannel* channel, HRESULT result) = 0;
    virtual void OnTransacted(NamedPipeChannel* channel, HRESULT result,
                              void* input, void* output, int length) = 0;
  };

  enum class EndPoint { kUnknown, kServer, kClient };

  NamedPipeChannel();
  ~NamedPipeChannel();

  HRESULT Create(const base::StringPiece16& name, DWORD mode,
                 DWORD max_instances, DWORD default_timeout);
  HRESULT ConnectAsync(Listener* listener);
  HRESULT Disconnect();
  HRESULT Impersonate();

  HRESULT Open(const base::StringPiece16& name, DWORD timeout);

  void Close() override;

  HRESULT ReadAsync(void* buffer, int length,
                    Channel::Listener* listener) override;
  HRESULT WriteAsync(const void* buffer, int length,
                     Channel::Listener* listener) override;
  HRESULT TransactAsync(const void* input, int input_length, void* output,
                        int output_length, Listener* listener);

  HRESULT Read(void* buffer, int* length);
  HRESULT Peek(void* buffer, int* length, DWORD* total_avail, DWORD* remaining);

  HRESULT GetMode(DWORD* mode) const;
  HRESULT SetMode(DWORD mode);
  HRESULT GetCurrentInstances(DWORD* count) const;
  HRESULT GetMaxInstances(DWORD* count) const;
  HRESULT GetClientUserName(wchar_t* user_name, DWORD user_name_size) const;

  EndPoint end_point() const {
    return end_point_;
  }

  bool IsValid() const {
    return work_ != nullptr && handle_ != INVALID_HANDLE_VALUE &&
           io_ != nullptr;
  }

 private:
  struct Request;

  HRESULT DispatchRequest(std::unique_ptr<Request>&& request);
  HRESULT GetHandleState(DWORD* mode, DWORD* instances, wchar_t* user_name,
                         DWORD user_name_size) const;

  static void CALLBACK OnRequested(PTP_CALLBACK_INSTANCE callback,
                                   void* context, PTP_WORK work);
  void OnRequested(PTP_WORK work);

  static void CALLBACK OnCompleted(PTP_CALLBACK_INSTANCE callback,
                                   void* context, void* overlapped, ULONG error,
                                   ULONG_PTR bytes, PTP_IO io);
  void OnCompleted(OVERLAPPED* overlapped, ULONG error, ULONG_PTR bytes);

  base::Lock lock_;
  PTP_WORK work_;
  std::queue<std::unique_ptr<Request>> queue_;

  HANDLE handle_;
  PTP_IO io_;
  EndPoint end_point_;

  NamedPipeChannel(const NamedPipeChannel&) = delete;
  NamedPipeChannel& operator=(const NamedPipeChannel&) = delete;
};

}  // namespace io
}  // namespace juno

#endif  // JUNO_IO_NAMED_PIPE_CHANNEL_H_
