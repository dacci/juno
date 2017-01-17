// Copyright (c) 2016 dacci.org

#ifndef JUNO_SERVICE_SCISSORS_SCISSORS_UNWRAPPING_SESSION_H_
#define JUNO_SERVICE_SCISSORS_SCISSORS_UNWRAPPING_SESSION_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "misc/timer_service.h"
#include "service/scissors/scissors.h"

namespace juno {
namespace io {
namespace net {

class DatagramChannel;

}  // namespace net
}  // namespace io

namespace service {
namespace scissors {

class ScissorsUnwrappingSession : public Scissors::Session,
                                  public io::Channel::Listener,
                                  public misc::TimerService::Callback {
 public:
  ScissorsUnwrappingSession(Scissors* service, const io::ChannelPtr& source);
  ~ScissorsUnwrappingSession();

  bool Start() override;
  void Stop() override;

  void OnRead(io::Channel* channel, HRESULT result, void* buffer,
              int length) override;
  void OnWritten(io::Channel* channel, HRESULT result, void* buffer,
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

  bool OnStreamRead(io::Channel* channel, HRESULT result, void* buffer,
                    int length);
  bool OnDatagramReceived(io::Channel* channel, HRESULT result, void* buffer,
                          int length);

  misc::TimerService::TimerObject timer_;

  std::shared_ptr<io::Channel> stream_;
  char stream_buffer_[4096];
  std::string stream_message_;

  std::shared_ptr<io::net::DatagramChannel> datagram_;
  char datagram_buffer_[kDataSize];

  ScissorsUnwrappingSession(const ScissorsUnwrappingSession&) = delete;
  ScissorsUnwrappingSession& operator=(const ScissorsUnwrappingSession&) =
      delete;
};

}  // namespace scissors
}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_SCISSORS_SCISSORS_UNWRAPPING_SESSION_H_
