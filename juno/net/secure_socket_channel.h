// Copyright (c) 2014 dacci.org

#ifndef JUNO_NET_SECURE_SOCKET_CHANNEL_H_
#define JUNO_NET_SECURE_SOCKET_CHANNEL_H_

#include <madoka/net/socket.h>

#include <base/synchronization/condition_variable.h>
#include <base/synchronization/lock.h>

#include <memory>
#include <string>
#include <vector>

#include "misc/schannel/schannel_context.h"
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
  class Request;
  class ReadRequest;
  class WriteRequest;

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
  base::Lock lock_;
  base::ConditionVariable empty_;

  INIT_ONCE init_once_;
  SecPkgContext_StreamSizes sizes_;
  HRESULT last_error_;
  std::string message_;
  std::string decrypted_;

  SecureSocketChannel(const SecureSocketChannel&);
  SecureSocketChannel& operator=(const SecureSocketChannel&);
};

#endif  // JUNO_NET_SECURE_SOCKET_CHANNEL_H_
