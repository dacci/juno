// Copyright (c) 2013 dacci.org

#include "service/http/http_request.h"

#include <picohttpparser/picohttpparser.h>

#include <string>

namespace juno {
namespace service {
namespace http {

HttpRequest::HttpRequest() : minor_version_(0) {}

int HttpRequest::Parse(const char* data, size_t length) {
  const char* method;
  size_t method_length;
  const char* path;
  size_t path_length;
  int minor_version;
  phr_header headers[kMaxHeaders];
  auto header_count = kMaxHeaders;

  auto result = phr_parse_request(data, length, &method, &method_length, &path,
                                  &path_length, &minor_version, headers,
                                  &header_count, 0);
  if (result > 0) {
    Clear();

    method_.assign(method, method_length);
    path_.assign(path, path_length);
    minor_version_ = minor_version;

    AddHeaders(headers, header_count);
  }

  return result;
}

void HttpRequest::Clear() {
  method_.clear();
  path_.clear();
  minor_version_ = -1;
  ClearHeaders();
}

void HttpRequest::Serialize(std::string* output) const {
  char version[12];
  sprintf_s(version, " HTTP/1.%d\x0D\x0A", minor_version_);

  output->append(method_).append(" ").append(path_).append(version);
  SerializeHeaders(output);
  output->append("\x0D\x0A");
}

}  // namespace http
}  // namespace service
}  // namespace juno
