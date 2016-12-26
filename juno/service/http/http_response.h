// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_HTTP_HTTP_RESPONSE_H_
#define JUNO_SERVICE_HTTP_HTTP_RESPONSE_H_

#include <string>

#include "service/http/http_headers.h"
#include "service/http/http_status.h"

namespace juno {
namespace service {
namespace http {

class HttpResponse : public HttpHeaders {
 public:
  static const int kPartial = -2;
  static const int kError = -1;

  HttpResponse();

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

  void SetStatus(StatusCode status) {
    SetStatus(status, GetStatusMessage(status));
  }

  void SetStatus(StatusCode status, const char* message) {
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

  HttpResponse(const HttpResponse&) = delete;
  HttpResponse& operator=(const HttpResponse&) = delete;
};

}  // namespace http
}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_HTTP_HTTP_RESPONSE_H_
