// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_HTTP_HTTP_REQUEST_H_
#define JUNO_NET_HTTP_HTTP_REQUEST_H_

#include <string>

#include "net/http/http_headers.h"

class HttpRequest : public HttpHeaders {
 public:
  static const int kPartial = -2;
  static const int kError = -1;

  HttpRequest();
  virtual ~HttpRequest();

  int Parse(const char* data, size_t length);
  int Parse(const std::string& data) {
    return Parse(data.data(), data.size());
  }

  void Clear();

  const std::string& method() const {
    return method_;
  }

  const std::string& path() const {
    return path_;
  }

  int minor_version() const {
    return minor_version_;
  }

 private:
  static const size_t kMaxHeaders = 128;

  std::string method_;
  std::string path_;
  int minor_version_;
};

#endif  // JUNO_NET_HTTP_HTTP_REQUEST_H_
