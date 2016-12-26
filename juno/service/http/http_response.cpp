// Copyright (c) 2013 dacci.org

#include "service/http/http_response.h"

#include <picohttpparser/picohttpparser.h>

#include <string>

namespace juno {
namespace service {
namespace http {

HttpResponse::HttpResponse() : minor_version_(0), status_(0) {}

int HttpResponse::Parse(const char* data, size_t length) {
  int minor_version;
  int status;
  const char* message;
  size_t message_length;
  phr_header headers[kMaxHeaders];
  auto header_count = kMaxHeaders;

  auto result =
      phr_parse_response(data, length, &minor_version, &status, &message,
                         &message_length, headers, &header_count, 0);
  if (result > 0) {
    Clear();

    minor_version_ = minor_version;
    status_ = status;
    message_.assign(message, message_length);

    AddHeaders(headers, header_count);
  }

  return result;
}

void HttpResponse::Clear() {
  minor_version_ = -1;
  status_ = -1;
  message_.clear();
  ClearHeaders();
}

void HttpResponse::Serialize(std::string* output) {
  char status[14];
  sprintf_s(status, "HTTP/1.%d %d ", minor_version_, status_);

  output->append(status).append(message_).append("\x0D\x0A");
  SerializeHeaders(output);
  output->append("\x0D\x0A");
}

}  // namespace http
}  // namespace service
}  // namespace juno
