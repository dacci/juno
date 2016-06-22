// Copyright (c) 2015 dacci.org

#include "service/scissors/scissors_unwrapping_session.h"

#include <base/logging.h>

#include <memory>

#include "service/scissors/scissors_wrapping_session.h"

using ::madoka::net::AsyncSocket;

ScissorsUnwrappingSession::ScissorsUnwrappingSession(Scissors* service)
    : Session(service), packet_length_(-1) {
  DLOG(INFO) << this << " session created";
}

ScissorsUnwrappingSession::~ScissorsUnwrappingSession() {
  Stop();

  DLOG(INFO) << this << " session destroyed";
}

bool ScissorsUnwrappingSession::Start() {
  if (source_ == nullptr) {
    LOG(ERROR) << this << " source is not specified";
    return false;
  }

  if (sink_ == nullptr) {
    sink_ = service_->CreateSocket();
    if (sink_ == nullptr) {
      LOG(ERROR) << this << " failed to create sink";
      return false;
    }

    auto wrapper = std::make_unique<ScissorsWrappingSession>(service_);
    if (wrapper == nullptr) {
      LOG(ERROR) << this << " failed to create wrapping session";
      return false;
    }

    wrapper->SetSource(sink_);
    wrapper->SetSink(source_);

    if (!service_->StartSession(std::move(wrapper))) {
      LOG(ERROR) << this << " failed to start wrapping session";
      service_->EndSession(this);
    }
  }

  auto result = source_->ReadAsync(buffer_, sizeof(buffer_), this);
  if (FAILED(result)) {
    LOG(ERROR) << this
        << " failed to read from source: 0x" << std::hex << result;
    service_->EndSession(this);
  }

  DLOG(INFO) << this << " session started";

  return true;
}

void ScissorsUnwrappingSession::Stop() {
  DLOG(INFO) << this << " stop requested";

  source_->Close();
}

void ScissorsUnwrappingSession::ProcessBuffer() {
  if (received_.size() < packet_length_) {
    auto result = source_->ReadAsync(buffer_, sizeof(buffer_), this);
    if (FAILED(result)) {
      LOG(ERROR) << this
          << " failed to read from source: 0x" << std::hex << result;
      service_->EndSession(this);
    }
  } else if (sink_address_length_ > 0) {
    sink_->SendToAsync(received_.data(), packet_length_, 0,
                       reinterpret_cast<sockaddr*>(&sink_address_),
                       sink_address_length_, this);
  } else {
    sink_->SendAsync(received_.data(), packet_length_, 0, this);
  }
}

void ScissorsUnwrappingSession::OnRead(Channel* channel, HRESULT result,
                                       void* buffer, int length) {
  if (FAILED(result) || length == 0) {
    LOG_IF(ERROR, FAILED(result)) << this
        << " failed to receive from the source: 0x" << std::hex << result;
    service_->EndSession(this);
    return;
  }

  DLOG(INFO) << this << " " << length << " bytes received from the source";

  char* pointer = static_cast<char*>(buffer);

  if (packet_length_ == -1) {
    memmove(&packet_length_, pointer, 2);
    packet_length_ = htons(packet_length_);

    pointer += 2;
    length -= 2;
  }

  received_.append(pointer, length);
  ProcessBuffer();
}

void ScissorsUnwrappingSession::OnWritten(Channel* channel, HRESULT result,
                                          void* buffer, int length) {
  DCHECK(false);
}

void ScissorsUnwrappingSession::OnSent(AsyncSocket* socket, HRESULT result,
                                       void* buffer, int length) {
  if (FAILED(result) || length == 0) {
    LOG_IF(ERROR, FAILED(result))
        << this << " failed to send to the sink: 0x" << std::hex << result;
    service_->EndSession(this);
    return;
  }

  DLOG(INFO) << this << " " << length << " bytes sent to the sink";

  received_.erase(0, length);
  if (received_.empty()) {
    packet_length_ = -1;

    result = source_->ReadAsync(buffer_, sizeof(buffer_), this);
    if (FAILED(result)) {
      LOG(ERROR) << this
          << " failed to read from source: 0x" << std::hex << result;
      service_->EndSession(this);
    }
  } else {
    ProcessBuffer();
  }
}

void ScissorsUnwrappingSession::OnSentTo(
    AsyncSocket* socket, HRESULT result, void* buffer, int length,
    const sockaddr* address, int address_length) {
  OnSent(socket, result, buffer, length);
}
