// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_SCISSORS_SCISSORS_TCP_SESSION_H_
#define JUNO_NET_SCISSORS_SCISSORS_TCP_SESSION_H_

#include <madoka/net/resolver.h>

#include <memory>
#include <vector>

#include "misc/schannel/security_buffer.h"
#include "net/async_socket.h"
#include "net/scissors/scissors.h"

class SchannelContext;

class ScissorsTcpSession
    : public Scissors::Session, public AsyncSocket::Listener {
 public:
  explicit ScissorsTcpSession(Scissors* service);
  virtual ~ScissorsTcpSession();

  bool Start(AsyncSocket* client);
  void Stop();

  void OnConnected(AsyncSocket* socket, DWORD error);
  void OnReceived(AsyncSocket* socket, DWORD error, int length);
  void OnSent(AsyncSocket* socket, DWORD error, int length);

 private:
  static const size_t kBufferSize = 8192;

  bool SendAsync(const AsyncSocketPtr& socket, const SecBuffer& buffer);
  bool DoNegotiation();
  bool CompleteNegotiation();
  bool DoEncryption();
  bool DoDecryption();
  void EndSession(AsyncSocket* socket);

  bool OnClientReceived(int length);
  bool OnRemoteReceived(int length);
  bool OnClientSent(int length);
  bool OnRemoteSent(int length);

  static void CALLBACK DeleteThis(PTP_CALLBACK_INSTANCE instance, void* param);

  Scissors* const service_;
  madoka::net::Resolver resolver_;
  AsyncSocketPtr client_;
  AsyncSocketPtr remote_;
  SchannelContext* context_;
  bool established_;
  LONG ref_count_;
  bool shutdown_;

  SecPkgContext_StreamSizes stream_sizes_;
  int negotiating_;

  std::vector<char> client_data_;
  std::unique_ptr<char[]> client_buffer_;

  std::vector<char> remote_data_;
  std::unique_ptr<char[]> remote_buffer_;

  SecurityBufferBundle token_input_;
  SecurityBufferBundle token_output_;
  SecurityBufferBundle encrypted_;
  SecurityBufferBundle decrypted_;
};

#endif  // JUNO_NET_SCISSORS_SCISSORS_TCP_SESSION_H_
