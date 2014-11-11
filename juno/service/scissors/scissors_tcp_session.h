// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SCISSORS_SCISSORS_TCP_SESSION_H_
#define JUNO_SERVICE_SCISSORS_SCISSORS_TCP_SESSION_H_

#include <madoka/net/resolver.h>
#include <madoka/net/socket_event_listener.h>

#include <memory>
#include <vector>

#include "misc/security/security_buffer.h"
#include "service/scissors/scissors.h"

class SchannelContext;

class ScissorsTcpSession
    : public Scissors::Session, public madoka::net::SocketEventAdapter {
 public:
  explicit ScissorsTcpSession(Scissors* service);
  virtual ~ScissorsTcpSession();

  bool Start(const Service::AsyncSocketPtr& client);
  void Stop() override;

  void OnConnected(madoka::net::AsyncSocket* socket, DWORD error) override;
  void OnReceived(madoka::net::AsyncSocket* socket, DWORD error, void* buffer,
                  int length) override;
  void OnSent(madoka::net::AsyncSocket* socket, DWORD error, void* buffer,
              int length) override;

 private:
  static const size_t kBufferSize = 8192;

  void SendAsync(const Service::AsyncSocketPtr& socket,
                 const SecBuffer& buffer);
  bool DoNegotiation();
  bool CompleteNegotiation();
  bool DoEncryption();
  bool DoDecryption();
  void EndSession(madoka::net::AsyncSocket* socket);

  bool OnClientReceived(int length);
  bool OnRemoteReceived(int length);
  bool OnClientSent(int length);
  bool OnRemoteSent(int length);

  static void CALLBACK DeleteThis(PTP_CALLBACK_INSTANCE instance, void* param);

  Scissors* const service_;
  madoka::net::Resolver resolver_;
  Service::AsyncSocketPtr client_;
  Service::AsyncSocketPtr remote_;
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

#endif  // JUNO_SERVICE_SCISSORS_SCISSORS_TCP_SESSION_H_
