// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_SCISSORS_SCISSORS_UDP_SESSION_H_
#define JUNO_SERVICE_SCISSORS_SCISSORS_UDP_SESSION_H_

#include <memory>

#include "io/net/datagram_channel.h"
#include "misc/timer_service.h"
#include "service/scissors/scissors.h"
#include "service/service.h"

class ScissorsUdpSession : public Scissors::UdpSession,
                           private Channel::Listener,
                           private TimerService::Callback {
 public:
  ScissorsUdpSession(Scissors* service,
                     const std::shared_ptr<DatagramChannel>& source);
  ~ScissorsUdpSession();

  bool Start() override;
  void Stop() override;

 private:
  static const size_t kBufferSize = 64 * 1024;  // 64 KiB
  static const int kTimeout = 15 * 1000;        // 15 sec

  void OnReceived(const DatagramPtr& datagram) override;

  void OnRead(Channel* channel, HRESULT result, void* buffer,
              int length) override;
  void OnWritten(Channel* channel, HRESULT result, void* buffer,
                 int length) override;

  void OnTimeout() override;

  std::shared_ptr<DatagramChannel> source_;
  std::shared_ptr<DatagramChannel> sink_;
  sockaddr_storage address_;
  int address_length_;
  char buffer_[kBufferSize];
  TimerService::TimerObject timer_;

  ScissorsUdpSession(const ScissorsUdpSession&) = delete;
  ScissorsUdpSession& operator=(const ScissorsUdpSession&) = delete;
};

#endif  // JUNO_SERVICE_SCISSORS_SCISSORS_UDP_SESSION_H_
