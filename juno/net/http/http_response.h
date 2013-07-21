// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_HTTP_HTTP_RESPONSE_H_
#define JUNO_NET_HTTP_HTTP_RESPONSE_H_

#include <string>

#include "net/http/http_headers.h"

class HttpResponse : public HttpHeaders {
 public:
  static const int kPartial = -2;
  static const int kError = -1;

  HttpResponse();
  virtual ~HttpResponse();

  int Process(const char* data, size_t length);

  int minor_version() const {
    return minor_version_;
  }

  int status() const {
    return status_;
  }

  const std::string& message() const {
    return message_;
  }

  std::string buffer_;

 private:
  static const size_t kMaxHeaders = 128;

  int minor_version_;
  int status_;
  std::string message_;
};

#endif  // JUNO_NET_HTTP_HTTP_RESPONSE_H_
