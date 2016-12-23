// Copyright (c) 2016 dacci.org

#include "service/scissors/scissors_wrapping_session.h"

#include <base/logging.h>

#include "net/datagram.h"
#include "net/datagram_channel.h"

ScissorsWrappingSession::ScissorsWrappingSession(Scissors* service,
                                                 const Datagram* datagram)
    : UdpSession(service),
      connected_(false),
      timer_(TimerService::GetDefault()->Create(this)),
      address_length_(0) {
  if (datagram == nullptr)
    return;

  datagram_ = datagram->channel;
  address_length_ = datagram->from_length;
  memcpy(&address_, &datagram->from, datagram->from_length);
}

ScissorsWrappingSession::~ScissorsWrappingSession() {
  Stop();
}

bool ScissorsWrappingSession::Start() {
  if (timer_ == nullptr) {
    LOG(ERROR) << "Failed to create timer.";
    return false;
  }

  if (datagram_ == nullptr) {
    LOG(ERROR) << "Datagram is null.";
    return false;
  }

  if (address_length_ == 0) {
    LOG(ERROR) << "Remote address is invalid.";
    return false;
  }

  auto remote = std::make_shared<SocketChannel>();
  if (remote == nullptr) {
    LOG(ERROR) << "Failed to create socket.";
    return false;
  }

  stream_ = service_->CreateChannel(remote);
  if (stream_ == nullptr) {
    LOG(ERROR) << "Failed to create channel.";
    return false;
  }

  auto result = service_->ConnectSocket(remote.get(), this);
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to connect: 0x" << std::hex << result;
    return false;
  }

  return true;
}

void ScissorsWrappingSession::Stop() {
  if (timer_ != nullptr)
    timer_->Stop();

  if (stream_ != nullptr)
    stream_->Close();
}

void ScissorsWrappingSession::OnReceived(const DatagramPtr& datagram) {
  timer_->Start(kTimeout, 0);

  if (datagram->data_length <= kDataSize) {
    base::AutoLock guard(lock_);
    queue_.push_back(std::move(datagram));
  }

  SendDatagram();
}

void ScissorsWrappingSession::OnRead(Channel* /*channel*/, HRESULT result,
                                     void* /*buffer*/, int length) {
  auto succeeded = false;

  do {
    if (FAILED(result) || length == 0) {
      LOG_IF(ERROR, FAILED(result)) << "Failed to read from stream: 0x"
                                    << std::hex << result;
      DLOG_IF(WARNING, length == 0) << "stream disconnected.";
      break;
    }

    stream_message_.append(stream_buffer_, length);

    if (stream_message_.size() < kHeaderSize) {
      DLOG(INFO) << "Incomplete message.";
      succeeded = true;
      break;
    }

    auto packet = reinterpret_cast<const Packet*>(stream_message_.data());
    length = _byteswap_ushort(packet->length);
    if (static_cast<int>(stream_message_.size()) < kHeaderSize + length) {
      DLOG(INFO) << "Incomplete payload.";
      succeeded = true;
      break;
    }

    auto buffer = std::make_unique<char[]>(length);
    if (buffer == nullptr) {
      LOG(ERROR) << "Failed to allocate buffer.";
      break;
    }

    memcpy(buffer.get(), packet->data, length);
    stream_message_.erase(0, kHeaderSize + length);

    result = datagram_->WriteAsync(buffer.get(), length, &address_,
                                   address_length_, this);
    if (SUCCEEDED(result)) {
      buffer.release();
    } else {
      LOG(ERROR) << "Failed to send datagram: 0x" << std::hex << result;
      break;
    }
  } while (true);

  if (succeeded) {
    result = stream_->ReadAsync(stream_buffer_, sizeof(stream_buffer_), this);
    if (FAILED(result)) {
      LOG(ERROR) << "Failed to read from stream: 0x" << std::hex << result;
      succeeded = false;
    }
  }

  if (!succeeded)
    service_->EndSession(this);
}

void ScissorsWrappingSession::OnWritten(Channel* channel, HRESULT result,
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

void ScissorsWrappingSession::OnConnected(SocketChannel* /*channel*/,
                                          HRESULT result) {
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to connect: 0x" << std::hex << result;
    service_->EndSession(this);
    return;
  }

  {
    base::AutoLock guard(lock_);
    DCHECK(!queue_.empty()) << "This must not occur.";
    connected_ = true;
  }

  result = stream_->ReadAsync(stream_buffer_, sizeof(stream_buffer_), this);
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to read stream: 0x" << std::hex << result;
    service_->EndSession(this);
    return;
  }

  SendDatagram();
}

void ScissorsWrappingSession::OnTimeout() {
  LOG(INFO) << "Timeout.";
  timer_->Stop();
  stream_->Close();
}

void ScissorsWrappingSession::SendDatagram() {
  static auto sending = false;

  base::AutoLock guard(lock_);

  if (!connected_)
    return;

  if (!sending)
    sending = true;
  else
    return;

  auto failed = false;

  while (true) {
    if (queue_.empty())
      break;

    auto datagram = std::move(queue_.front());
    queue_.pop_front();

    base::AutoUnlock unlock(lock_);

    auto buffer = std::make_unique<char[]>(kHeaderSize + datagram->data_length);
    if (buffer == nullptr) {
      failed = true;
      LOG(ERROR) << "Failed to allocate buffer.";
      break;
    }

    auto packet = reinterpret_cast<Packet*>(buffer.get());
    packet->length =
        _byteswap_ushort(static_cast<uint16_t>(datagram->data_length));
    memcpy(packet->data, datagram->data.get(), datagram->data_length);

    auto result = stream_->WriteAsync(
        buffer.get(), kHeaderSize + datagram->data_length, this);
    if (SUCCEEDED(result)) {
      buffer.release();
    } else {
      failed = true;
      LOG(ERROR) << "Failed to send datagram: 0x" << std::hex << result;
      break;
    }
  }

  sending = false;

  if (failed)
    service_->EndSession(this);
}
