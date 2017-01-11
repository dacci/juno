// Copyright (c) 2016 dacci.org

#ifndef JUNO_SERVICE_SOCKS_SOCKS_SESSION_4_H_
#define JUNO_SERVICE_SOCKS_SOCKS_SESSION_4_H_

#include <memory>
#include <string>

#include "io/net/socket_channel.h"

#include "service/socks/socks4.h"
#include "service/socks/socks_proxy.h"

namespace juno {
namespace service {
namespace socks {

class SocksSession4 : public SocksSession,
                      private io::Channel::Listener,
                      private io::net::SocketChannel::Listener {
 public:
  SocksSession4(SocksProxy* proxy, const io::ChannelPtr& channel);
  ~SocksSession4();

  HRESULT Start(std::unique_ptr<char[]>&& request, int length) override;
  void Stop() override;

 private:
  static const int kBufferSize = 1024;

  HRESULT ProcessRequest();
  HRESULT SendResponse(SOCKS4::CODE code);

  void OnRead(io::Channel* channel, HRESULT result, void* buffer,
              int length) override;
  void OnWritten(io::Channel* channel, HRESULT result, void* buffer,
                 int length) override;
  void OnConnected(io::net::SocketChannel* channel, HRESULT result) override;
  void OnClosed(io::net::SocketChannel* channel, HRESULT result) override;

  std::string message_;
  void* end_point_;
  std::shared_ptr<io::net::SocketChannel> remote_;

  SocksSession4(const SocksSession4&) = delete;
  SocksSession4& operator=(const SocksSession4&) = delete;
};

}  // namespace socks
}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_SOCKS_SOCKS_SESSION_4_H_
