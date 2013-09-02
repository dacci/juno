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
  explicit ScissorsSession(Scissors* service);
  virtual ~ScissorsSession();

  bool Start(AsyncSocket* client);
  void Stop();

  void OnConnected(AsyncSocket* socket, DWORD error);
  void OnReceived(AsyncSocket* socket, DWORD error, int length);
  void OnSent(AsyncSocket* socket, DWORD error, int length);

 private:
  static const size_t kBufferSize = 8192;

  bool SendAsync(AsyncSocket* socket, const SecBuffer& buffer);
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
  AsyncSocket* client_;
  AsyncSocket* remote_;
  SchannelContext* context_;
  bool established_;
  LONG ref_count_;

  SecPkgContext_StreamSizes stream_sizes_;
  int negotiating_;

  std::vector<char> client_data_;
  char* client_buffer_;

  std::vector<char> remote_data_;
  char* remote_buffer_;

  SecurityBufferBundle token_input_;
  SecurityBufferBundle token_output_;
  SecurityBufferBundle encrypted_;
  SecurityBufferBundle decrypted_;
};

#endif  // JUNO_NET_SCISSORS_SCISSORS_SESSION_H_
