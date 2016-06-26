// Copyright (c) 2015 dacci.org

#ifndef JUNO_SERVICE_SCISSORS_SCISSORS_WRAPPING_SESSION_H_
#define JUNO_SERVICE_SCISSORS_SCISSORS_WRAPPING_SESSION_H_

#include <memory>

#include "misc/timer_service.h"
#include "net/channel.h"
#include "service/scissors/scissors.h"

class ScissorsUnwrappingSession;

class ScissorsWrappingSession : public Scissors::UdpSession,
                                private Channel::Listener,
                                private SocketChannel::Listener,
                                private madoka::net::AsyncSocket::Listener,
                                private TimerService::Callback {
 public:
  typedef std::shared_ptr<madoka::net::AsyncSocket> AsyncSocketPtr;

  explicit ScissorsWrappingSession(Scissors* service);
  ~ScissorsWrappingSession();

  bool Start() override;
  void Stop() override;

  void SetSource(const AsyncSocketPtr& source) {
    source_ = source;
  }

  void SetSink(const Service::ChannelPtr& sink) {
    sink_ = sink;
  }

  void SetSourceAddress(const sockaddr_storage& address, int length) {
    memmove(&source_address_, &address, length);
    source_address_length_ = length;
  }

 private:
  static const int kLengthOffset = 2;
  static const int kDatagramSize = 65535;
  static const int kTimeout = 60 * 1000;  // 60 sec.

  void OnReceived(const Service::DatagramPtr& datagram) override;

  void OnConnected(SocketChannel* socket, HRESULT result) override;
  void OnReceived(madoka::net::AsyncSocket* socket, HRESULT result,
                  void* buffer, int length, int flags) override;
  void OnClosed(SocketChannel* /*channel*/, HRESULT /*result*/) override {}

  void OnRead(Channel* channel, HRESULT result, void* buffer,
              int length) override;
  void OnWritten(Channel* channel, HRESULT result, void* buffer,
                 int length) override;

  void OnConnected(madoka::net::AsyncSocket* /*socket*/, HRESULT /*result*/,
                   const addrinfo* /*end_point*/) override {}
  void OnReceivedFrom(madoka::net::AsyncSocket* /*socket*/, HRESULT /*result*/,
                      void* /*buffer*/, int /*length*/, int /*flags*/,
                      const sockaddr* /*address*/,
                      int /*address_length*/) override {}
  void OnSent(madoka::net::AsyncSocket* /*socket*/, HRESULT /*result*/,
              void* /*buffer*/, int /*length*/) override {}
  void OnSentTo(madoka::net::AsyncSocket* /*socket*/, HRESULT /*result*/,
                void* /*buffer*/, int /*length*/, const sockaddr* /*address*/,
                int /*address_length*/) override {}

  void OnTimeout() override;

  TimerService::TimerObject timer_;
  AsyncSocketPtr source_;
  Service::ChannelPtr sink_;
  sockaddr_storage source_address_;
  int source_address_length_;
  char buffer_[kLengthOffset + kDatagramSize];

  ScissorsWrappingSession(const ScissorsWrappingSession&) = delete;
  ScissorsWrappingSession& operator=(const ScissorsWrappingSession&) = delete;
};

#endif  // JUNO_SERVICE_SCISSORS_SCISSORS_WRAPPING_SESSION_H_
