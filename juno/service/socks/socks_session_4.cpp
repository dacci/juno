// Copyright (c) 2016 dacci.org

#include "service/socks/socks_session_4.h"

#include <base/logging.h>

#include "net/socket_resolver.h"
#include "net/tunneling_service.h"
#include "service/socks/socket_address.h"

SocksSession4::SocksSession4(SocksProxy* proxy, const ChannelPtr& channel)
    : SocksSession(proxy, channel), end_point_(nullptr) {}

SocksSession4::~SocksSession4() {
  Stop();
}

HRESULT SocksSession4::Start(std::unique_ptr<char[]>&& request, int length) {
  if (!message_.empty()) {
    LOG(ERROR) << "Already started.";
    return E_ILLEGAL_METHOD_CALL;
  }

  message_.assign(request.get(), length);

  return ProcessRequest();
}

void SocksSession4::Stop() {
  if (client_ != nullptr)
    client_->Close();

  if (remote_ != nullptr)
    remote_->Close();
}

HRESULT SocksSession4::ProcessRequest() {
  if (message_.size() < sizeof(SOCKS4::REQUEST) || message_.back() != '\0') {
    auto buffer = std::make_unique<char[]>(kBufferSize);
    if (buffer == nullptr) {
      LOG(ERROR) << "Failed to allocate buffer.";
      return E_OUTOFMEMORY;
    }

    auto result = client_->ReadAsync(buffer.get(), kBufferSize, this);
    if (FAILED(result)) {
      LOG(ERROR) << "Failed to receive: 0x" << std::hex << result;
      return result;
    }

    buffer.release();
    return S_OK;
  }

  auto request = reinterpret_cast<const SOCKS4::REQUEST*>(message_.data());
  auto code = SOCKS4::FAILED;

  do {
    if (request->command != SOCKS4::CONNECT) {
      LOG(ERROR) << "Command " << request->command << " is requested.";
      break;
    }

    remote_ = std::make_shared<SocketChannel>();
    if (remote_ == nullptr) {
      LOG(ERROR) << "Failed to allocate SocketChannel.";
      break;
    }

    if (request->address.s_addr != 0 &&
        htonl(request->address.s_addr) <= 0x000000FF) {
      // SOCKS4a extension
      auto host = request->user_id + strlen(request->user_id) + 1;

      auto resolver = std::make_unique<SocketResolver>();
      if (resolver == nullptr) {
        LOG(ERROR) << "Failed to allocate SocketResolver.";
        break;
      }

      auto result = resolver->Resolve(host, htons(request->port));
      if (FAILED(result)) {
        LOG(ERROR) << "Failed to resolve " << host << ": 0x" << std::hex
                   << result;
        break;
      }

      end_point_ = resolver.get();

      result = remote_->ConnectAsync(resolver->begin()->get(), this);
      if (FAILED(result)) {
        LOG(ERROR) << "Failed to connect: 0x" << std::hex << result;
        break;
      }

      resolver.release();
    } else {
      auto address = std::make_unique<SocketAddress4>();
      if (address == nullptr) {
        LOG(ERROR) << "Failed to allocate SocketAddress4.";
        break;
      }

      address->ai_socktype = SOCK_STREAM;
      address->sin_port = request->port;
      address->sin_addr = request->address;

      end_point_ = address.get();

      auto result = remote_->ConnectAsync(address.get(), this);
      if (FAILED(result)) {
        LOG(ERROR) << "Failed to connect: 0x" << std::hex << result;
        break;
      }

      address.release();
    }

    return S_OK;
  } while (false);

  return SendResponse(code);
}

HRESULT SocksSession4::SendResponse(SOCKS4::CODE code) {
  auto response = std::make_unique<SOCKS4::RESPONSE>();
  memset(response.get(), 0, sizeof(*response));
  response->code = code;

  auto result = client_->WriteAsync(response.get(), sizeof(*response), this);
  if (SUCCEEDED(result))
    response.release();
  else
    LOG(ERROR) << "Failed to send: 0x" << std::hex << result;

  return result;
}

void SocksSession4::OnRead(Channel* channel, HRESULT result, void* buffer,
                           int length) {
  DCHECK(channel == client_.get());

  std::unique_ptr<char[]> message(static_cast<char*>(buffer));

  do {
    if (FAILED(result)) {
      LOG(ERROR) << "Failed to receive: 0x" << std::hex << result;
      break;
    }

    if (length <= 0) {
      LOG(WARNING) << "Channel closed.";
      break;
    }

    message_.append(message.get(), length);

    result = ProcessRequest();
    if (FAILED(result)) {
      LOG(ERROR) << "Failed to process request: 0x" << std::hex << result;
      break;
    }

    return;
  } while (false);

  proxy_->EndSession(this);
}

void SocksSession4::OnWritten(Channel* channel, HRESULT result, void* buffer,
                              int length) {
  DCHECK(channel == client_.get());

  std::unique_ptr<SOCKS4::RESPONSE> response(
      static_cast<SOCKS4::RESPONSE*>(buffer));

  do {
    if (FAILED(result)) {
      LOG(ERROR) << "Failed to send: 0x" << std::hex << result;
      break;
    }

    if (length <= 0) {
      LOG(WARNING) << "Channel closed.";
      break;
    }

    if (response->code == SOCKS4::GRANTED) {
      client_.reset();
      remote_.reset();
    }
  } while (false);

  proxy_->EndSession(this);
}

void SocksSession4::OnConnected(SocketChannel* channel, HRESULT result) {
  DCHECK(channel == remote_.get());

  auto request = reinterpret_cast<const SOCKS4::REQUEST*>(message_.data());
  if (request->address.s_addr != 0 &&
      htonl(request->address.s_addr) <= 0x000000FF)
    delete static_cast<SocketResolver*>(end_point_);
  else
    delete static_cast<SocketAddress4*>(end_point_);

  SOCKS4::CODE code;
  if (SUCCEEDED(result) && TunnelingService::Bind(client_, remote_))
    code = SOCKS4::GRANTED;
  else
    code = SOCKS4::FAILED;

  result = SendResponse(code);
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to send response: 0x" << std::hex << result;
    proxy_->EndSession(this);
  }
}

void SocksSession4::OnClosed(SocketChannel* channel, HRESULT result) {
  LOG(FATAL) << "channel: " << channel << ", result: 0x" << std::hex << result;
}
