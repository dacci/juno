// Copyright (c) 2016 dacci.org

#ifndef JUNO_SERVICE_SCISSORS_SCISSORS_UNWRAPPING_SESSION_H_
#define JUNO_SERVICE_SCISSORS_SCISSORS_UNWRAPPING_SESSION_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "misc/timer_service.h"
#include "service/scissors/scissors.h"

class DatagramChannel;
class Scissors;

class ScissorsUnwrappingSession : public Scissors::Session,
                                  public Channel::Listener,
                                  public TimerService::Callback {
 public:
  ScissorsUnwrappingSession(Scissors* service, const ChannelPtr& source);
  ~ScissorsUnwrappingSession();

  bool Start() override;
  void Stop() override;

  void OnRead(Channel* channel, HRESULT result, void* buffer,
              int length) override;
  void OnWritten(Channel* channel, HRESULT result, void* buffer,
                 int length) override;

  void OnTimeout() override;

 private:
  struct Packet {
    uint16_t length;
    char data[ANYSIZE_ARRAY];
  };

  static const int kHeaderSize = 2;
  static const int kDataSize = 0xFFFF;
  static const int kTimeout = 60 * 1000;

  bool OnStreamRead(Channel* channel, HRESULT result, void* buffer, int length);
  bool OnDatagramReceived(Channel* channel, HRESULT result, void* buffer,
                          int length);

  TimerService::TimerObject timer_;

  std::shared_ptr<Channel> stream_;
  char stream_buffer_[4096];
  std::string stream_message_;

  std::shared_ptr<DatagramChannel> datagram_;
  char datagram_buffer_[kDataSize];

  ScissorsUnwrappingSession(const ScissorsUnwrappingSession&) = delete;
  ScissorsUnwrappingSession& operator=(const ScissorsUnwrappingSession&) =
      delete;
};

#endif  // JUNO_SERVICE_SCISSORS_SCISSORS_UNWRAPPING_SESSION_H_
