// Copyright (c) 2014 dacci.org

#ifndef JUNO_NET_SECURE_SOCKET_CHANNEL_H_
#define JUNO_NET_SECURE_SOCKET_CHANNEL_H_

#include <madoka/concurrent/condition_variable.h>
#include <madoka/concurrent/critical_section.h>
#include <madoka/net/socket.h>

#include <memory>
#include <string>
#include <vector>

#include "misc/security/schannel_context.h"
#include "net/channel.h"

class SecureSocketChannel : public Channel {
 public:
  typedef std::shared_ptr<madoka::net::Socket> SocketPtr;

  SecureSocketChannel(SchannelCredential* credential, const SocketPtr& socket,
                      bool inbound);
  virtual ~SecureSocketChannel();

  void Close() override;
  void ReadAsync(void* buffer, int length, Listener* listener) override;
  void WriteAsync(const void* buffer, int length, Listener* listener) override;

  SchannelContext* context() {
    return &context_;
  }

 private:
  class Request {
   public:
    Request(SecureSocketChannel* channel, void* buffer, int length,
            Listener* listener)
        : channel_(channel), buffer_(buffer), length_(length),
          listener_(listener) {
    }

    virtual void Run() = 0;

    inline void FireReadError(DWORD error) {
      listener_->OnRead(channel_, error, buffer_, 0);
    }

    inline void FireWriteError(DWORD error) {
      listener_->OnWritten(channel_, error, buffer_, 0);
    }

    SecureSocketChannel* const channel_;
    void* const buffer_;
    const int length_;
    Listener* const listener_;
  };

  class ReadRequest : public Request {
   public:
    ReadRequest(SecureSocketChannel* channel, void* buffer, int length,
                Listener* listener);

    void Run() override;
  };

  class WriteRequest : public Request {
   public:
    WriteRequest(SecureSocketChannel* channel, void* buffer, int length,
                 Listener* listener);

    void Run() override;
  };

  static void CALLBACK BeginRequest(PTP_CALLBACK_INSTANCE instance,
                                    void* param);
  void EndRequest(Request* request);

  static BOOL CALLBACK InitOnceCallback(PINIT_ONCE init_once, void* parameter,
                                        void** context);
  BOOL InitializeInbound(void** context);
  BOOL InitializeOutbound(void** context);
  HRESULT EnsureNegotiated();
  HRESULT Negotiate();
  HRESULT DecryptMessage(PSecBufferDesc message);
  void Shutdown();

  SchannelContext context_;
  SocketPtr socket_;
  const bool inbound_;
  bool closed_;

  std::vector<std::unique_ptr<Request>> requests_;
  madoka::concurrent::CriticalSection lock_;
  madoka::concurrent::ConditionVariable empty_;

  INIT_ONCE init_once_;
  SecPkgContext_StreamSizes sizes_;
  HRESULT last_error_;
  std::string message_;
  std::string decrypted_;

  SecureSocketChannel(const SecureSocketChannel&);
  SecureSocketChannel& operator=(const SecureSocketChannel&);
};

#endif  // JUNO_NET_SECURE_SOCKET_CHANNEL_H_
