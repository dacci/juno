// Copyright (c) 2016 dacci.org

#ifndef JUNO_SERVICE_SCISSORS_SCISSORS_WRAPPING_SESSION_H_
#define JUNO_SERVICE_SCISSORS_SCISSORS_WRAPPING_SESSION_H_

#include <base/synchronization/lock.h>

#include <list>
#include <string>

#include "misc/timer_service.h"
#include "service/scissors/scissors.h"

class ScissorsWrappingSession : public Scissors::UdpSession,
                                public Channel::Listener,
                                public SocketChannel::Listener,
                                public TimerService::Callback {
 public:
  ScissorsWrappingSession(Scissors* service, const Datagram* datagram);
  ~ScissorsWrappingSession();

  bool Start() override;
  void Stop() override;

  void OnReceived(const Service::DatagramPtr& datagram) override;
  void OnRead(Channel* channel, HRESULT result, void* buffer,
              int length) override;
  void OnWritten(Channel* channel, HRESULT result, void* buffer,
                 int length) override;

  void OnConnected(SocketChannel* channel, HRESULT result) override;
  void OnClosed(SocketChannel* /*channel*/, HRESULT /*result*/) override {}

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
  std::list<Service::DatagramPtr> queue_;
  bool connected_;
  TimerService::TimerObject timer_;

  std::shared_ptr<Channel> stream_;
  char stream_buffer_[4096];
  std::string stream_message_;

  std::shared_ptr<DatagramChannel> datagram_;
  int address_length_;
  sockaddr_storage address_;

  ScissorsWrappingSession(const ScissorsWrappingSession&) = delete;
  ScissorsWrappingSession& operator=(const ScissorsWrappingSession&) = delete;
};

#endif  // JUNO_SERVICE_SCISSORS_SCISSORS_WRAPPING_SESSION_H_
