// Copyright (c) 2015 dacci.org

#include "service/scissors/scissors_wrapping_session.h"

#include <stdint.h>

#include <madoka/net/async_socket.h>

#include <base/logging.h>

#include "net/datagram.h"
#include "net/datagram_channel.h"
#include "service/scissors/scissors_config.h"
#include "service/scissors/scissors_unwrapping_session.h"

using ::madoka::net::AsyncDatagramSocket;
using ::madoka::net::AsyncSocket;

ScissorsWrappingSession::ScissorsWrappingSession(Scissors* service)
    : UdpSession(service), source_address_length_(0) {
  DLOG(INFO) << this << " session created";
}

ScissorsWrappingSession::~ScissorsWrappingSession() {
  if (source_address_length_ == 0)
    source_->Close();

  DLOG(INFO) << this << " session destroyed";
}

bool ScissorsWrappingSession::Start() {
  if (source_ == nullptr) {
    LOG(ERROR) << this << " source is not specified";
    return false;
  }

  timer_ = TimerService::GetDefault()->Create(this);
  if (timer_ == nullptr) {
    LOG(ERROR) << this << " failed to create timer";
    return false;
  }

  if (sink_ == nullptr) {
    auto socket = std::make_shared<AsyncSocket>();
    if (socket == nullptr) {
      LOG(ERROR) << this << " failed to create socket";
      return false;
    }

    sink_ = service_->CreateChannel(socket);
    if (sink_ == nullptr) {
      LOG(ERROR) << this << " failed to create channel";
      return false;
    }

    service_->ConnectSocket(socket.get(), this);
  }

  if (source_address_length_ == 0) {
    timer_->Start(kTimeout, 0);
    source_->ReceiveAsync(buffer_ + kLengthOffset, kDatagramSize, 0, this);
  }

  DLOG(INFO) << this << " session started";

  return true;
}

void ScissorsWrappingSession::Stop() {
  DLOG(INFO) << this << " stop requested";

  timer_->Stop();
  sink_->Close();

  service_->EndSession(this);
}

void ScissorsWrappingSession::OnReceived(const Service::DatagramPtr& datagram) {
  if (datagram->data_length > kDatagramSize) {
    LOG(WARNING) << this << " datagram too large, dropping";
    return;
  }

  OnReceived(source_.get(), 0, datagram->data.get(), datagram->data_length);
}

void ScissorsWrappingSession::OnConnected(AsyncSocket* socket, DWORD error) {
  if (error != 0) {
    LOG(ERROR) << this << " failed to connect: " << error;
    Stop();
    return;
  }

  DLOG(INFO) << this << " connected to the sink";

  auto pair = std::make_unique<ScissorsUnwrappingSession>(service_);
  if (pair == nullptr) {
    LOG(ERROR) << this << " failed to create unwrapping session";
    Stop();
    return;
  }

  pair->SetSource(sink_);
  pair->SetSink(source_);
  if (source_address_length_ > 0)
    pair->SetSinkAddress(source_address_, source_address_length_);

  service_->StartSession(std::move(pair));
}

void ScissorsWrappingSession::OnReceived(AsyncDatagramSocket* /*socket*/,
                                         DWORD error, void* buffer,
                                         int length) {
  timer_->Stop();

  if (error != 0) {
    LOG(ERROR) << this << " failed to receive from the source: " << error;
    Stop();
    return;
  }

  DLOG(INFO) << this << " " << length << " bytes received";

  memmove(buffer_ + kLengthOffset, buffer, length);

  uint16_t* packet_length = reinterpret_cast<uint16_t*>(buffer_);
  *packet_length = htons(length);

  sink_->WriteAsync(buffer_, kLengthOffset + length, this);
}

void ScissorsWrappingSession::OnRead(Channel* channel, DWORD error,
                                     void* buffer, int length) {
  DCHECK(false);
}

void ScissorsWrappingSession::OnWritten(Channel* channel, DWORD error,
                                        void* buffer, int length) {
  if (error == 0 && length > 0) {
    timer_->Start(kTimeout, 0);

    if (source_address_length_ == 0)
      source_->ReceiveAsync(buffer_ + kLengthOffset, kDatagramSize, 0, this);
  } else {
    LOG(ERROR) << this << " failed to send to sink: " << error;
    Stop();
  }
}

void ScissorsWrappingSession::OnTimeout() {
  LOG(WARNING) << this << " timeout";

  Stop();
}
