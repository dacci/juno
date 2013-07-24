// Copyright (c) 2013 dacci.org

#include <stdint.h>

#include <atlbase.h>
#include <atlutil.h>

#include <madoka/net/winsock.h>

#include <string>

#include "net/async_socket.h"
#include "net/async_server_socket.h"
#include "net/http/http_request.h"
#include "net/http/http_response.h"

class Event {
 public:
  Event() : handle_(::CreateEvent(NULL, TRUE, FALSE, NULL)) {
  }

  ~Event() {
    if (handle_)
      ::CloseHandle(handle_);
  }

  bool Wait() {
    return ::WaitForSingleObject(handle_, INFINITE) == WAIT_OBJECT_0;
  }

  operator HANDLE() const {
    return handle_;
  }

 private:
  HANDLE const handle_;

  Event(const Event&);
  Event& operator=(const Event&);
};

DWORD CALLBACK ThreadProc(void* param) {
  AsyncSocket* client = static_cast<AsyncSocket*>(param);
  HttpRequest request;
  char buffer[4096];
  Event event;

  int result = HttpRequest::kError;

  while (true) {
    OVERLAPPED* context = client->BeginReceive(buffer, sizeof(buffer), 0,
                                               event);
    if (context == NULL)
      break;

    int length = client->EndReceive(context);
    if (length <= 0)
      break;

    result = request.Process(buffer, length);
    if (result != HttpRequest::kPartial)
      break;
  }
  if (result < 0) {
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

  madoka::net::AddressInfo resolver;
  resolver.ai_socktype = SOCK_STREAM;
  if (!resolver.Resolve(url.GetHostName(), service)) {
    client->Shutdown(SD_BOTH);
    delete client;
    return __LINE__;
  }

  AsyncSocket remote;
#if 0
  for (auto i = resolver.begin(), l = resolver.end(); i != l; ++i) {
    if (remote.Connect(*i))
      break;
  }
#else
  OVERLAPPED* context = remote.BeginConnect(&resolver, event);
  if (context == NULL) {
    client->Shutdown(SD_BOTH);
    delete client;
    return __LINE__;
  }

  remote.EndConnect(context);
#endif

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

class Listener : public AsyncServerSocket::Listener {
 public:
  explicit Listener(AsyncServerSocket* server) : server_(server) {
  }

  void OnAccepted(AsyncSocket* client, DWORD error) {
    ::fprintf(stderr, "line:%u\tfunc:%s\tclient:%p\terror:%lu\n",
              __LINE__, __FUNCTION__, client, error);
    if (error == 0) {
      server_->AcceptAsync(this);

      HANDLE thread = ::CreateThread(NULL, 0, ThreadProc, client, 0, NULL);
      if (thread != NULL)
        ::CloseHandle(thread);
      else
        delete client;
    } else {
      delete client;
    }
  }

  AsyncServerSocket* const server_;
};

int main(int argc, char* argv[]) {
  const char* port = "8080";
  if (argc >= 2)
    port = argv[1];

  const char* address = "127.0.0.1";
  if (argc >= 3)
    address = argv[2];

  madoka::net::WinSock winsock(WINSOCK_VERSION);
  if (!winsock.Initialized())
    return winsock.error();

  madoka::net::AddressInfo address_info;
  address_info.ai_flags = AI_PASSIVE;
  address_info.ai_socktype = SOCK_STREAM;
  if (!address_info.Resolve(address, port))
    return __LINE__;

  AsyncServerSocket server;
  for (auto i = address_info.begin(), l = address_info.end(); i != l; ++i) {
    if (server.Bind(*i))
      break;
  }
  if (!server.bound())
    return __LINE__;

  if (!server.Listen(SOMAXCONN))
    return __LINE__;

#if 0
  HANDLE event = ::CreateEvent(NULL, TRUE, FALSE, NULL);

  while (true) {
    OVERLAPPED* overlapped = server.BeginAccept(event);
    if (overlapped == NULL)
      return __LINE__;

    AsyncSocket* client = server.EndAccept(overlapped);
    HANDLE thread = ::CreateThread(NULL, 0, ThreadProc, client, 0, NULL);
    if (thread == NULL) {
      delete client;
      return __LINE__;
    } else {
      ::CloseHandle(thread);
    }
  }

  ::CloseHandle(event);
#else
  Listener listener(&server);
  if (!server.AcceptAsync(&listener))
    return __LINE__;

  ::getchar();

  server.Close();
#endif

  return 0;
}
