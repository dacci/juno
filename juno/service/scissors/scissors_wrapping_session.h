// Copyright (c) 2016 dacci.org

#ifndef JUNO_SERVICE_SCISSORS_SCISSORS_WRAPPING_SESSION_H_
#define JUNO_SERVICE_SCISSORS_SCISSORS_WRAPPING_SESSION_H_

#include <base/synchronization/lock.h>

#include <list>
#include <string>

#include "misc/timer_service.h"
#include "service/scissors/scissors.h"

namespace juno {
namespace service {
namespace scissors {

class ScissorsWrappingSession : public Scissors::UdpSession,
                                public io::Channel::Listener,
                                public io::net::SocketChannel::Listener,
                                public misc::TimerService::Callback {
 public:
  ScissorsWrappingSession(Scissors* service, const io::net::Datagram* datagram);
  ~ScissorsWrappingSession();

  bool Start() override;
  void Stop() override;

  void OnReceived(std::unique_ptr<io::net::Datagram>&& datagram) override;
  void OnRead(io::Channel* channel, HRESULT result, void* buffer,
              int length) override;
  void OnWritten(io::Channel* channel, HRESULT result, void* buffer,
                 int length) override;

  void OnConnected(io::net::SocketChannel* channel, HRESULT result) override;
  void OnClosed(io::net::SocketChannel* /*channel*/,
                HRESULT /*result*/) override {}

  void OnTimeout() override;

 private:
  struct Packet {
    uint16_t length;
    char data[ANYSIZE_ARRAY];
  };

  static const int kHeaderSize = 2;
  static const int kDataSize = 0xFFFF;
  static const int kTimeout = 5 * 1000;

  void SendDatagram();

  base::Lock lock_;
  std::list<std::unique_ptr<io::net::Datagram>> queue_;
  bool connected_;
  std::unique_ptr<misc::TimerService::Timer> timer_;

  std::shared_ptr<io::Channel> stream_;
  char stream_buffer_[4096];
  std::string stream_message_;

  std::shared_ptr<io::net::DatagramChannel> datagram_;
  int address_length_;
  sockaddr_storage address_;

  ScissorsWrappingSession(const ScissorsWrappingSession&) = delete;
  ScissorsWrappingSession& operator=(const ScissorsWrappingSession&) = delete;
};

}  // namespace scissors
}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_SCISSORS_SCISSORS_WRAPPING_SESSION_H_
