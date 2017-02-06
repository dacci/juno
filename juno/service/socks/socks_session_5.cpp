// Copyright (c) 2016 dacci.org

#include "service/socks/socks_session_5.h"

#include <base/logging.h>

#include <string>

#include "io/net/socket_resolver.h"
#include "misc/tunneling_service.h"

#include "service/socks/socket_address.h"
#include "service/socks/socks5.h"

namespace juno {
namespace service {
namespace socks {

SocksSession5::SocksSession5(SocksProxy* proxy,
                             std::unique_ptr<io::Channel>&& channel)
    : SocksSession(proxy, std::move(channel)),
      state_(State::kInit),
      end_point_(nullptr) {}

SocksSession5::~SocksSession5() {
  SocksSession5::Stop();
}

HRESULT SocksSession5::Start(std::unique_ptr<char[]>&& request, int length) {
  if (!message_.empty()) {
    LOG(ERROR) << "Already started.";
    return E_ILLEGAL_METHOD_CALL;
  }

  state_ = State::kNegotiation;
  message_.assign(request.get(), length);

  return ProcessRequest();
}

void SocksSession5::Stop() {
  if (client_ != nullptr)
    client_->Close();

  if (remote_ != nullptr)
    remote_->Close();
}

HRESULT SocksSession5::ReadRequest() {
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

HRESULT SocksSession5::EnsureRead(size_t min_length) {
  if (min_length <= message_.size())
    return S_OK;

  auto result = ReadRequest();
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to read request: 0x" << std::hex << result;
    return result;
  }

  return S_FALSE;
}

HRESULT SocksSession5::ProcessRequest() {
  switch (state_) {
    case State::kNegotiation:
      return ProcessNegotiation();

    case State::kCommand:
      return ProcessCommand();

    default:
      LOG(FATAL) << "Unknown state: " << static_cast<int>(state_);
      return E_UNEXPECTED;
  }
}

HRESULT SocksSession5::ProcessNegotiation() {
  auto result = EnsureRead(sizeof(SOCKS5::METHOD_REQUEST));
  if (result != S_OK) {
    if (FAILED(result))
      LOG(ERROR) << "Failed to ensure read: 0x" << std::hex << result;

    return result;
  }

  auto request =
      reinterpret_cast<const SOCKS5::METHOD_REQUEST*>(message_.data());
  result =
      EnsureRead(sizeof(SOCKS5::METHOD_REQUEST) - 1 + request->method_count);
  if (result != S_OK) {
    if (FAILED(result))
      LOG(ERROR) << "Failed to ensure read: 0x" << std::hex << result;

    return result;
  }

  auto method = SOCKS5::UNSUPPORTED;
  for (auto i = 0; i < request->method_count; ++i) {
    if (request->methods[i] == SOCKS5::NO_AUTH) {
      method = SOCKS5::NO_AUTH;
      break;
    }
  }

  auto buffer = std::make_unique<char[]>(sizeof(SOCKS5::METHOD_RESPONSE));
  if (buffer == nullptr) {
    LOG(ERROR) << "Failed to allocate buffer.";
    return E_OUTOFMEMORY;
  }

  auto response = reinterpret_cast<SOCKS5::METHOD_RESPONSE*>(buffer.get());
  response->version = 5;
  response->method = method;

  result = client_->WriteAsync(response, sizeof(*response), this);
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to send: 0x" << std::hex << result;
    return result;
  }

  buffer.release();
  return S_OK;
}

HRESULT SocksSession5::ProcessCommand() {
  auto result =
      EnsureRead(offsetof(SOCKS5::REQUEST, address.domain.domain_len));
  if (result != S_OK) {
    if (FAILED(result))
      LOG(ERROR) << "Failed to ensure read: 0x" << std::hex << result;

    return result;
  }

  auto request = reinterpret_cast<const SOCKS5::REQUEST*>(message_.data());
  if (request->command != SOCKS5::CONNECT) {
    auto buffer = std::make_unique<char[]>(sizeof(SOCKS5::RESPONSE));
    if (buffer == nullptr) {
      LOG(ERROR) << "Failed to allocate buffer.";
      return E_OUTOFMEMORY;
    }

    auto response = reinterpret_cast<SOCKS5::RESPONSE*>(buffer.get());
    memset(response, 0, sizeof(*response));
    response->version = 5;
    response->code = SOCKS5::COMMAND_NOT_SUPPORTED;

    result = client_->WriteAsync(response, sizeof(*response), this);
    if (FAILED(result)) {
      LOG(ERROR) << "Failed to send: 0x" << std::hex << result;
      return result;
    }

    buffer.release();
    return S_OK;
  }

  auto request_size = 4 + 2;  // common part + port number
  switch (request->type) {
    case SOCKS5::IP_V4:
      request_size += sizeof(SOCKS5::ADDRESS::ipv4.ipv4_addr);
      break;

    case SOCKS5::DOMAINNAME:
      request_size += request->address.domain.domain_len;
      break;

    case SOCKS5::IP_V6:
      request_size += sizeof(SOCKS5::ADDRESS::ipv6.ipv6_addr);
      break;

    default:
      LOG(ERROR) << "Illegal address type: " << request->type;
      return E_INVALID_PROTOCOL_FORMAT;
  }

  result = EnsureRead(request_size);
  if (result != S_OK) {
    if (FAILED(result))
      LOG(ERROR) << "Failed to ensure read: 0x" << std::hex << result;

    return result;
  }

  remote_ = std::make_shared<io::net::SocketChannel>();
  if (remote_ == nullptr) {
    LOG(ERROR) << "Failed to allocate SocketChannel.";
    return E_OUTOFMEMORY;
  }

  if (request->type == SOCKS5::IP_V4) {
    auto address = std::make_unique<SocketAddress4>();
    if (address == nullptr) {
      LOG(ERROR) << "Failed to allocate SocketAddress4.";
      return E_OUTOFMEMORY;
    }

    address->ai_socktype = SOCK_STREAM;
    address->sin_port = request->address.ipv4.ipv4_port;
    address->sin_addr = request->address.ipv4.ipv4_addr;

    end_point_ = address.get();

    result = remote_->ConnectAsync(address.get(), this);
    if (FAILED(result)) {
      LOG(ERROR) << "Failed to connect: 0x" << std::hex << result;
      return result;
    }

    address.release();
    return S_OK;
  }

  if (request->type == SOCKS5::DOMAINNAME) {
    std::string host(request->address.domain.domain_name,
                     request->address.domain.domain_len);
    auto port =
        *reinterpret_cast<const uint16_t*>(request->address.domain.domain_name +
                                           request->address.domain.domain_len);

    auto resolver = std::make_unique<io::net::SocketResolver>();
    if (resolver == nullptr) {
      LOG(ERROR) << "Failed to allocate SocketResolver.";
      return E_OUTOFMEMORY;
    }

    result = resolver->Resolve(host.c_str(), htons(port));
    if (FAILED(result)) {
      LOG(ERROR) << "Failed to resolve " << host << ": 0x" << std::hex
                 << result;
      return result;
    }

    end_point_ = resolver.get();

    result = remote_->ConnectAsync(resolver->begin()->get(), this);
    if (FAILED(result)) {
      LOG(ERROR) << "Failed to connect: 0x" << std::hex << result;
      return result;
    }

    resolver.release();
    return S_OK;
  }

  if (request->type == SOCKS5::IP_V6) {
    auto address = std::make_unique<SocketAddress6>();
    if (address == nullptr) {
      LOG(ERROR) << "Failed to allocate SocketAddress6.";
      return E_OUTOFMEMORY;
    }

    address->ai_socktype = SOCK_STREAM;
    address->sin6_port = request->address.ipv6.ipv6_port;
    address->sin6_addr = request->address.ipv6.ipv6_addr;

    end_point_ = address.get();

    result = remote_->ConnectAsync(address.get(), this);
    if (FAILED(result)) {
      LOG(ERROR) << "Failed to connect: 0x" << std::hex << result;
      return result;
    }

    address.release();
    return S_OK;
  }

  LOG(ERROR) << "Invalid address type: " << request->type;
  return E_INVALID_PROTOCOL_FORMAT;
}

void SocksSession5::OnRead(io::Channel* channel, HRESULT result, void* buffer,
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

void SocksSession5::OnWritten(io::Channel* channel, HRESULT result,
                              void* buffer, int length) {
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

    switch (state_) {
      case State::kNegotiation:
        state_ = State::kCommand;
        message_.clear();

        result = ReadRequest();
        if (FAILED(result)) {
          LOG(ERROR) << "Failed to read request: 0x" << std::hex << result;
          break;
        }
        return;

      case State::kCommand: {
        auto response = static_cast<SOCKS5::RESPONSE*>(buffer);
        if (response->code == SOCKS5::SUCCEEDED) {
          client_.reset();
          remote_.reset();
        }
        break;
      }

      default:
        LOG(FATAL) << "Unknown state: " << static_cast<int>(state_);
        break;
    }
  } while (false);

  proxy_->EndSession(this);
}

void SocksSession5::OnConnected(io::net::SocketChannel* channel,
                                HRESULT result) {
  DCHECK(channel == remote_.get());

  auto request = reinterpret_cast<const SOCKS5::REQUEST*>(message_.data());
  switch (request->type) {
    case SOCKS5::IP_V4:
      delete static_cast<SocketAddress4*>(end_point_);
      break;

    case SOCKS5::DOMAINNAME:
      delete static_cast<io::net::SocketResolver*>(end_point_);
      break;

    case SOCKS5::IP_V6:
      delete static_cast<SocketAddress6*>(end_point_);
      break;

    default:
      LOG(FATAL) << "Unknown address type: " << request->type;
      break;
  }

  SOCKS5::CODE code;
  if (SUCCEEDED(result) && misc::TunnelingService::Bind(client_, remote_))
    code = SOCKS5::SUCCEEDED;
  else
    code = SOCKS5::GENERAL_FAILURE;

  auto buffer = std::make_unique<char[]>(sizeof(SOCKS5::RESPONSE));
  if (buffer == nullptr) {
    LOG(ERROR) << "Failed to allocate buffer.";
    proxy_->EndSession(this);
    return;
  }

  auto response = reinterpret_cast<SOCKS5::RESPONSE*>(buffer.get());
  memset(response, 0, sizeof(*response));
  response->version = 5;
  response->code = code;

  sockaddr_storage address;
  int length = sizeof(address);
  if (remote_->GetLocalEndPoint(&address, &length)) {
    length = offsetof(SOCKS5::RESPONSE, address);

    switch (address.ss_family) {
      case AF_INET: {
        auto address4 = reinterpret_cast<sockaddr_in*>(&address);
        response->type = SOCKS5::IP_V4;
        response->address.ipv4.ipv4_addr = address4->sin_addr;
        response->address.ipv4.ipv4_port = address4->sin_port;
        length += sizeof(SOCKS5::ADDRESS::ipv4);
        break;
      }

      case AF_INET6: {
        auto address6 = reinterpret_cast<sockaddr_in6*>(&address);
        response->type = SOCKS5::IP_V6;
        response->address.ipv6.ipv6_addr = address6->sin6_addr;
        response->address.ipv6.ipv6_port = address6->sin6_port;
        length += sizeof(SOCKS5::ADDRESS::ipv6);
        break;
      }

      default:
        response->type = SOCKS5::IP_V4;
        length += sizeof(SOCKS5::ADDRESS::ipv4);
        break;
    }
  } else {
    response->type = SOCKS5::IP_V4;
    length =
        offsetof(SOCKS5::RESPONSE, address) + sizeof(SOCKS5::ADDRESS::ipv4);
  }

  result = client_->WriteAsync(response, length, this);
  if (FAILED(result)) {
    LOG(ERROR) << "Failed to send: 0x" << std::hex << result;
    proxy_->EndSession(this);
    return;
  }

  buffer.release();
}

void SocksSession5::OnClosed(io::net::SocketChannel* channel, HRESULT result) {
  LOG(FATAL) << "channel: " << channel << ", result: 0x" << std::hex << result;
}

}  // namespace socks
}  // namespace service
}  // namespace juno
