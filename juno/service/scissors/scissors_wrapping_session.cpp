// Copyright (c) 2015 dacci.org

#include "service/scissors/scissors_wrapping_session.h"

#include <stdint.h>

#include <base/logging.h>

#include "net/datagram.h"
#include "service/scissors/scissors_unwrapping_session.h"

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
    auto socket = std::make_shared<SocketChannel>();
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

  OnReceived(source_.get(), S_OK, datagram->data.get(), datagram->data_length,
             0);
}

void ScissorsWrappingSession::OnConnected(SocketChannel* /*socket*/,
                                          HRESULT result) {
  if (FAILED(result)) {
    LOG(ERROR) << this << " failed to connect: 0x" << std::hex << result;
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

void ScissorsWrappingSession::OnReceived(AsyncSocket* /*socket*/,
                                         HRESULT result, void* buffer,
                                         int length, int /*flags*/) {
  timer_->Stop();

  if (FAILED(result)) {
    LOG(ERROR) << this << " failed to receive from the source: 0x" << std::hex
               << result;
    Stop();
    return;
  }

  DLOG(INFO) << this << " " << length << " bytes received";

  memmove(buffer_ + kLengthOffset, buffer, length);

  auto packet_length = reinterpret_cast<uint16_t*>(buffer_);
  *packet_length = htons(static_cast<u_short>(length));

  result = sink_->WriteAsync(buffer_, kLengthOffset + length, this);
  if (FAILED(result)) {
    LOG(ERROR) << this << " failed to send to sink: 0x" << std::hex << result;
    Stop();
  }
}

void ScissorsWrappingSession::OnRead(Channel* /*channel*/, HRESULT /*result*/,
                                     void* /*buffer*/, int /*length*/) {
  DCHECK(false);
}

void ScissorsWrappingSession::OnWritten(Channel* /*channel*/, HRESULT result,
                                        void* /*buffer*/, int length) {
  if (SUCCEEDED(result) && length > 0) {
    timer_->Start(kTimeout, 0);

    if (source_address_length_ == 0)
      source_->ReceiveAsync(buffer_ + kLengthOffset, kDatagramSize, 0, this);
  } else {
    LOG(ERROR) << this << " failed to send to sink: 0x" << std::hex << result;
    Stop();
  }
}

void ScissorsWrappingSession::OnTimeout() {
  LOG(WARNING) << this << " timeout";

  Stop();
}
