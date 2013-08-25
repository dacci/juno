// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_SCISSORS_SCISSORS_SESSION_H_
#define JUNO_NET_SCISSORS_SCISSORS_SESSION_H_

#include <vector>

#include "misc/schannel/security_buffer.h"
#include "net/async_socket.h"

class SchannelContext;
class Scissors;

class ScissorsSession : public AsyncSocket::Listener {
 public:
  ScissorsSession(Scissors* service, AsyncSocket* client);
  virtual ~ScissorsSession();

  bool Start();

  void OnConnected(AsyncSocket* socket, DWORD error);
  void OnReceived(AsyncSocket* socket, DWORD error, int length);
  void OnSent(AsyncSocket* socket, DWORD error, int length);

 private:
  static const size_t kBufferSize = 8192;

  bool SendAsync(AsyncSocket* socket, const SecBuffer& buffer);

  void OnClientReceived(DWORD error, int length);
  void OnRemoteReceived(DWORD error, int length);
  void OnClientSent(DWORD error, int length);
  void OnRemoteSent(DWORD error, int length);

  Scissors* const service_;
  AsyncSocket* client_;
  AsyncSocket* remote_;
  SchannelContext* context_;
};

#endif  // JUNO_NET_SCISSORS_SCISSORS_SESSION_H_
