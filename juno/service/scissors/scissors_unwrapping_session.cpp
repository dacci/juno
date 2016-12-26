// Copyright (c) 2016 dacci.org

#include "service/scissors/scissors_unwrapping_session.h"

#include <base/logging.h>

#include "io/net/datagram_channel.h"

namespace juno {
namespace service {
namespace scissors {

using ::juno::io::Channel;

ScissorsUnwrappingSession::ScissorsUnwrappingSession(
    Scissors* service, const io::ChannelPtr& source)
    : Session(service),
      timer_(misc::TimerService::GetDefault()->Create(this)),
      stream_(source) {}

ScissorsUnwrappingSession::~ScissorsUnwrappingSession() {
  Stop();
}

bool ScissorsUnwrappingSession::Start() {
  if (timer_ == nullptr) {
    LOG(ERROR) << "Failed to create timer.";
    return false;
  }

  if (stream_ == nullptr) {
    LOG(ERROR) << "Stream is null.";
    return false;
  }

  datagram_ = service_->CreateSocket();
  if (datagram_ == nullptr) {
    LOG(ERROR) << "Failed to create datagram channel.";
    return false;
  }

  HRESULT result;

  timer_->Start(kTimeout, 0);
  result = datagram_->ReadAsync(datagram_buffer_ + kHeaderSize,
                                sizeof(datagram_buffer_) - kHeaderSize, this);
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to receive datagram: 0x" << std::hex << result;
    return false;
  }

  result = stream_->ReadAsync(stream_buffer_, sizeof(stream_buffer_), this);
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to read from stream: 0x" << std::hex << result;
    return false;
  }

  return true;
}

void ScissorsUnwrappingSession::Stop() {
  if (timer_ != nullptr)
    timer_->Stop();

  if (datagram_ != nullptr)
    datagram_->Close();

  if (stream_ != nullptr)
    stream_->Close();
}

void ScissorsUnwrappingSession::OnRead(Channel* channel, HRESULT result,
                                       void* buffer, int length) {
  auto succeeded = false;
  if (channel == stream_.get())
    succeeded = OnStreamRead(channel, result, buffer, length);
  else
    succeeded = OnDatagramReceived(channel, result, buffer, length);

  if (!succeeded)
    service_->EndSession(this);
}

void ScissorsUnwrappingSession::OnWritten(Channel* channel, HRESULT result,
                                          void* buffer, int /*length*/) {
  delete[] static_cast<char*>(buffer);

  if (FAILED(result)) {
    if (channel == datagram_.get())
      LOG(ERROR) << "Failed to send datagram: 0x" << std::hex << result;
    else
      LOG(ERROR) << "Failed to write to stream: 0x" << std::hex << result;

    service_->EndSession(this);
  }
}

void ScissorsUnwrappingSession::OnTimeout() {
  LOG(INFO) << "Timeout.";
  timer_->Stop();
  datagram_->Close();
}

bool ScissorsUnwrappingSession::OnStreamRead(Channel* /*channel*/,
                                             HRESULT result, void* /*buffer*/,
                                             int length) {
  if (FAILED(result) || length == 0) {
    LOG_IF(ERROR, FAILED(result)) << "Failed to read from stream: 0x"
                                  << std::hex << result;
    DLOG_IF(WARNING, length == 0) << "stream disconnected.";
    return false;
  }

  stream_message_.append(stream_buffer_, length);

  do {
    if (stream_message_.size() < kHeaderSize) {
      DLOG(INFO) << "Incomplete message.";
      break;
    }

    auto packet = reinterpret_cast<const Packet*>(stream_message_.data());
    length = _byteswap_ushort(packet->length);
    if (static_cast<int>(stream_message_.size()) < kHeaderSize + length) {
      DLOG(INFO) << "Incomplete payload.";
      break;
    }

    auto buffer = std::make_unique<char[]>(length);
    if (buffer == nullptr) {
      LOG(ERROR) << "Failed to allocate buffer.";
      return false;
    }

    memcpy(buffer.get(), packet->data, length);
    stream_message_.erase(0, kHeaderSize + length);

    result = datagram_->WriteAsync(buffer.get(), length, this);
    if (SUCCEEDED(result)) {
      buffer.release();
    } else {
      LOG(ERROR) << "Failed to send datagram: 0x" << std::hex << result;
      return false;
    }
  } while (true);

  result = stream_->ReadAsync(stream_buffer_, sizeof(stream_buffer_), this);
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to read from stream: 0x" << std::hex << result;
    return false;
  }

  return true;
}

bool ScissorsUnwrappingSession::OnDatagramReceived(Channel* /*channel*/,
                                                   HRESULT result,
                                                   void* /*buffer*/,
                                                   int length) {
  timer_->Stop();

  if (FAILED(result)) {
    LOG(ERROR) << "Failed to receive datagram: 0x" << std::hex << result;
    return false;
  }

  auto packet = reinterpret_cast<Packet*>(datagram_buffer_);
  packet->length = _byteswap_ushort(static_cast<uint16_t>(length));

  auto buffer = std::make_unique<char[]>(kHeaderSize + length);
  if (buffer == nullptr) {
    LOG(ERROR) << "Failed to allocate buffer.";
    return false;
  }

  memcpy(buffer.get(), packet, kHeaderSize + length);

  result = stream_->WriteAsync(buffer.get(), kHeaderSize + length, this);
  if (SUCCEEDED(result)) {
    buffer.release();
  } else {
    LOG(ERROR) << "Failed to write to stream: 0x" << std::hex << result;
    return false;
  }

  timer_->Start(kTimeout, 0);
  result =
      datagram_->ReadAsync(datagram_buffer_, sizeof(datagram_buffer_), this);
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to receive datagram: 0x" << std::hex << result;
    return false;
  }

  return true;
}

}  // namespace scissors
}  // namespace service
}  // namespace juno
