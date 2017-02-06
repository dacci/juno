// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SCISSORS_SCISSORS_TCP_SESSION_H_
#define JUNO_SERVICE_SCISSORS_SCISSORS_TCP_SESSION_H_

#include "service/scissors/scissors.h"

namespace juno {
namespace service {
namespace scissors {

class ScissorsTcpSession : public Scissors::Session,
                           private io::net::SocketChannel::Listener {
 public:
  ScissorsTcpSession(Scissors* service, std::unique_ptr<io::Channel>&& source);
  ~ScissorsTcpSession();

  bool Start() override;
  void Stop() override;

  void OnConnected(io::net::SocketChannel* channel, HRESULT result) override;
  void OnClosed(io::net::SocketChannel* channel, HRESULT result) override;

 private:
  static const size_t kBufferSize = 8192;

  std::shared_ptr<io::Channel> source_;
  std::shared_ptr<io::Channel> sink_;

  ScissorsTcpSession(const ScissorsTcpSession&) = delete;
  ScissorsTcpSession& operator=(const ScissorsTcpSession&) = delete;
};

}  // namespace scissors
}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_SCISSORS_SCISSORS_TCP_SESSION_H_
