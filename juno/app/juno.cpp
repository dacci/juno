// Copyright (c) 2013 dacci.org

#include <stdint.h>

#include <atlbase.h>
#include <atlutil.h>

#include <madoka/net/server_socket.h>
#include <madoka/net/winsock.h>

#include "net/http/http_request.h"
#include "net/http/http_response.h"

namespace madonet = madoka::net;

DWORD CALLBACK ThreadProc(void* param) {
  madonet::Socket* client = static_cast<madonet::Socket*>(param);
  HttpRequest request;
  char buffer[4096];

  int result = 0;

  while (true) {
    int length = client->Receive(buffer, sizeof(buffer), 0);
    if (length <= 0)
      break;

    result = request.Process(buffer, length);
    if (result != HttpRequest::kPartial)
      break;
  }

  if (result == HttpRequest::kError) {
    client->Shutdown(SD_BOTH);
    delete client;
    return __LINE__;
  }

  int64_t content_length = 0;
  bool chunked = false;
  if (request.HeaderExists("Content-Length")) {
    const std::string& value = request.GetHeader("Content-Length");
    content_length = ::_strtoi64(value.c_str(), NULL, 10);
  } else if (request.HeaderExists("Transfer-Encoding")) {
    chunked = true;
  }

  // temporary
  if (chunked) {
    client->Shutdown(SD_BOTH);
    delete client;
    return __LINE__;
  }

  CUrl url;
  url.CrackUrl(request.path().c_str());
  char service[8];
  ::sprintf_s(service, "%d", url.GetPortNumber());

  madonet::AddressInfo resolver;
  resolver.ai_socktype = SOCK_STREAM;
  if (!resolver.Resolve(url.GetHostName(), service)) {
    client->Shutdown(SD_BOTH);
    delete client;
    return __LINE__;
  }

  madonet::Socket remote;
  for (const addrinfo* end_point : resolver) {
    if (remote.Connect(end_point))
      break;
  }
  if (!remote.connected()) {
    client->Shutdown(SD_BOTH);
    delete client;
    return __LINE__;
  }

  std::string remote_request;
  remote_request += request.method();
  remote_request += ' ';
  remote_request += url.GetUrlPath();
  remote_request += " HTTP/1.";
  remote_request += request.minor_version() + '0';
  remote_request += "\x0D\x0A";
  request.SerializeHeaders(&remote_request);
  remote_request += "\x0D\x0A";

  remote.Send(remote_request.c_str(), remote_request.size(), 0);
  ::printf("%s\n", remote_request.c_str());

  if (!request.buffer_.empty()) {
    remote.Send(request.buffer_.c_str(), request.buffer_.size(), 0);
    content_length -= request.buffer_.size();
  }

  while (content_length > 0) {
    int length = client->Receive(buffer, sizeof(buffer), 0);
    if (length <= 0)
      break;

    remote.Send(buffer, length, 0);
    content_length -= length;
  }

  HttpResponse response;

  while (true) {
    int length = remote.Receive(buffer, sizeof(buffer), 0);
    if (length <= 0)
      break;

    result = response.Process(buffer, length);
    if (result != HttpResponse::kPartial)
      break;
  }

  content_length = 0;
  chunked = false;
  if (response.HeaderExists("Content-Length")) {
    const std::string& value = response.GetHeader("Content-Length");
    content_length = ::_strtoi64(value.c_str(), NULL, 10);
  } else if (response.HeaderExists("Transfer-Encoding")) {
    chunked = true;
  }

  // temporary
  if (chunked) {
    client->Shutdown(SD_BOTH);
    delete client;
    return __LINE__;
  }

  std::string remote_response;
  remote_response += "HTTP/1.";
  remote_response += response.minor_version() + '0';
  remote_response += ' ';
  ::sprintf_s(buffer, "%d", response.status());
  remote_response += buffer;
  remote_response += ' ';
  remote_response += response.message();
  remote_response += "\x0D\x0A";
  response.SerializeHeaders(&remote_response);
  remote_response += "\x0D\x0A";

  client->Send(remote_response.c_str(), remote_response.size(), 0);
  ::printf("%s\n", remote_response.c_str());

  if (!response.buffer_.empty()) {
    client->Send(response.buffer_.c_str(), response.buffer_.size(), 0);
    content_length -= response.buffer_.size();
  }

  while (content_length > 0) {
    int length = remote.Receive(buffer, sizeof(buffer), 0);
    if (length <= 0)
      break;

    client->Send(buffer, length, 0);
    content_length -= length;
  }

  remote.Shutdown(SD_BOTH);
  client->Shutdown(SD_BOTH);

  remote.Close();
  delete client;

  return 0;
}

int wmain(int, wchar_t**) {
  madonet::WinSock winsock(WINSOCK_VERSION);
  if (!winsock.Initialized())
    return winsock.error();

  madonet::ServerSocket server;
  if (!server.Bind("127.0.0.1", 8080))
    return __LINE__;

  if (!server.Listen(SOMAXCONN))
    return __LINE__;

  while (true) {
    madonet::Socket* client = server.Accept();
    if (client == NULL)
      return __LINE__;

    HANDLE thread = ::CreateThread(NULL, 0, ThreadProc, client, 0, NULL);
    if (thread == NULL) {
      delete client;
      return __LINE__;
    } else {
      ::CloseHandle(thread);
    }
  }

  return 0;
}
