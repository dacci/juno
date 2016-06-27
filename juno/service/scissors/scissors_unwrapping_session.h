// Copyright (c) 2015 dacci.org

#ifndef JUNO_SERVICE_SCISSORS_SCISSORS_UNWRAPPING_SESSION_H_
#define JUNO_SERVICE_SCISSORS_SCISSORS_UNWRAPPING_SESSION_H_

#include <string>

#include "net/channel.h"
#include "service/scissors/scissors.h"

class ScissorsUnwrappingSession : public Scissors::Session,
                                  private Channel::Listener {
 public:
  explicit ScissorsUnwrappingSession(Scissors* service);
  ~ScissorsUnwrappingSession();

  bool Start() override;
  void Stop() override;

  void SetSource(const Service::ChannelPtr& source) {
    source_ = source;
  }

  void SetSink(const std::shared_ptr<DatagramChannel>& sink) {
    sink_ = sink;
  }

  void SetSinkAddress(const sockaddr_storage& address, int length) {
    memmove(&sink_address_, &address, length);
    sink_address_length_ = length;
  }

 private:
  static const int kBufferSize = 65535;

  void ProcessBuffer();

  void OnRead(Channel* channel, HRESULT result, void* buffer,
              int length) override;
  void OnWritten(Channel* channel, HRESULT result, void* buffer,
                 int length) override;

  Service::ChannelPtr source_;
  std::shared_ptr<DatagramChannel> sink_;
  sockaddr_storage sink_address_;
  int sink_address_length_;
  char buffer_[2 + kBufferSize];  // header + payload
  int packet_length_;
  std::string received_;

  ScissorsUnwrappingSession(const ScissorsUnwrappingSession&) = delete;
  ScissorsUnwrappingSession& operator=(const ScissorsUnwrappingSession&) =
      delete;
};

#endif  // JUNO_SERVICE_SCISSORS_SCISSORS_UNWRAPPING_SESSION_H_
