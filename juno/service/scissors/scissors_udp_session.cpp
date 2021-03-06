// Copyright (c) 2013 dacci.org

#include "service/scissors/scissors_udp_session.h"

#include <base/logging.h>

#include "io/net/datagram.h"

namespace juno {
namespace service {
namespace scissors {

ScissorsUdpSession::ScissorsUdpSession(
    Scissors* service, const std::shared_ptr<io::net::DatagramChannel>& source)
    : UdpSession(service), source_(source) {
  DLOG(INFO) << this << " session created";
}

ScissorsUdpSession::~ScissorsUdpSession() {
  DLOG(INFO) << this << " session destroyed";

  source_->Close();
  sink_->Close();
}

bool ScissorsUdpSession::Start() {
  timer_ = misc::TimerService::GetDefault()->Create(this);
  if (timer_ == nullptr) {
    LOG(ERROR) << this << " failed to create timer";
    return false;
  }

  sink_ = service_->CreateSocket();
  if (sink_ == nullptr) {
    LOG(ERROR) << this << " failed to create sink";
    return false;
  }

  timer_->Start(kTimeout, 0);
  sink_->ReadAsync(buffer_, sizeof(buffer_), this);

  DLOG(INFO) << this << " session started";

  return true;
}

void ScissorsUdpSession::Stop() {
  DLOG(INFO) << this << " stop requested";

  timer_->Stop();
  service_->EndSession(this);
}

void ScissorsUdpSession::OnReceived(
    std::unique_ptr<io::net::Datagram>&& datagram) {
  DLOG(INFO) << this << " " << datagram->data_length
             << " bytes receved from the source";

  timer_->Stop();

  memmove(&address_, &datagram->from, datagram->from_length);
  address_length_ = datagram->from_length;

  auto sent = sink_->Send(datagram->data.get(), datagram->data_length, 0);
  if (sent == datagram->data_length) {
    DLOG(INFO) << this << " " << sent << " bytes sent to the sink";
    timer_->Start(kTimeout, 0);
  } else {
    LOG(ERROR) << this << " failed to send to the sink";
    Stop();
  }
}

void ScissorsUdpSession::OnRead(io::Channel* /*channel*/, HRESULT result,
                                void* buffer, int length) {
  timer_->Stop();

  if (SUCCEEDED(result)) {
    DLOG(INFO) << this << " " << length << " bytes received from the sink";

    auto sent = source_->SendTo(buffer, length, 0, &address_, address_length_);
    if (sent == length) {
      DLOG(INFO) << this << " " << sent << " bytes sent to the source";
      timer_->Start(kTimeout, 0);
      result = sink_->ReadAsync(buffer_, sizeof(buffer_), this);
      if (FAILED(result)) {
        LOG(ERROR) << this << " failed to received from the sink: 0x"
                   << std::hex << result;
        Stop();
      }
    } else {
      LOG(ERROR) << this
                 << " failed to send to the source: " << WSAGetLastError();
      Stop();
    }
  } else {
    LOG(ERROR) << this << " failed to received from the sink: 0x" << std::hex
               << result;
    Stop();
  }
}

void ScissorsUdpSession::OnWritten(io::Channel* /*channel*/, HRESULT /*result*/,
                                   void* /*buffer*/, int /*length*/) {
  LOG(ERROR) << "This must not occur.";
}

void ScissorsUdpSession::OnTimeout() {
  LOG(WARNING) << this << " timeout";

  sink_->Close();
}

}  // namespace scissors
}  // namespace service
}  // namespace juno
