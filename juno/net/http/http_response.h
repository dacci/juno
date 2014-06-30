// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_HTTP_HTTP_RESPONSE_H_
#define JUNO_NET_HTTP_HTTP_RESPONSE_H_

#include <string>

#include "net/http/http_headers.h"
#include "net/http/http_status.h"

class HttpResponse : public HttpHeaders {
 public:
  static const int kPartial = -2;
  static const int kError = -1;

  HttpResponse();
  virtual ~HttpResponse();

  int Parse(const char* data, size_t length);
  int Parse(const std::string& data) {
    return Parse(data.data(), data.size());
  }

  void Clear();

  void Serialize(std::string* output);

  int minor_version() const {
    return minor_version_;
  }

  void set_minor_version(int minor_version) {
    minor_version_ = minor_version;
  }

  int status() const {
    return status_;
  }

  const std::string& message() const {
    return message_;
  }

  void SetStatus(HTTP::StatusCode status) {
    SetStatus(status, HTTP::GetStatusMessage(status));
  }

  void SetStatus(HTTP::StatusCode status, const char* message) {
    if (minor_version_ == -1)
      set_minor_version(1);

    status_ = status;
    message_ = message;
  }

 private:
  static const size_t kMaxHeaders = 128;

  int minor_version_;
  int status_;
  std::string message_;
};

#endif  // JUNO_NET_HTTP_HTTP_RESPONSE_H_
