// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SCISSORS_SCISSORS_UDP_SESSION_H_
#define JUNO_SERVICE_SCISSORS_SCISSORS_UDP_SESSION_H_

#include <memory>

#include "io/net/datagram_channel.h"
#include "misc/timer_service.h"
#include "service/scissors/scissors.h"
#include "service/service.h"

namespace juno {
namespace service {
namespace scissors {

class ScissorsUdpSession : public Scissors::UdpSession,
                           private io::Channel::Listener,
                           private misc::TimerService::Callback {
 public:
  ScissorsUdpSession(Scissors* service,
                     const std::shared_ptr<io::net::DatagramChannel>& source);
  ~ScissorsUdpSession();

  bool Start() override;
  void Stop() override;

 private:
  static const size_t kBufferSize = 64 * 1024;  // 64 KiB
  static const int kTimeout = 15 * 1000;        // 15 sec

  void OnReceived(const io::net::DatagramPtr& datagram) override;

  void OnRead(io::Channel* channel, HRESULT result, void* buffer,
              int length) override;
  void OnWritten(io::Channel* channel, HRESULT result, void* buffer,
                 int length) override;

  void OnTimeout() override;

  std::shared_ptr<io::net::DatagramChannel> source_;
  std::shared_ptr<io::net::DatagramChannel> sink_;
  sockaddr_storage address_;
  int address_length_;
  char buffer_[kBufferSize];
  misc::TimerService::TimerObject timer_;

  ScissorsUdpSession(const ScissorsUdpSession&) = delete;
  ScissorsUdpSession& operator=(const ScissorsUdpSession&) = delete;
};

}  // namespace scissors
}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_SCISSORS_SCISSORS_UDP_SESSION_H_
