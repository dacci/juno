// Copyright (c) 2013 dacci.org

#include "net/http/http_request.h"

#include <picohttpparser/picohttpparser.h>

HttpRequest::HttpRequest() {
}

HttpRequest::~HttpRequest() {
}

int HttpRequest::Process(const char* data, size_t length) {
  size_t last_length = buffer_.size();
  buffer_.append(data, length);

  const char* method;
  size_t method_length;
  const char* path;
  size_t path_length;
  int minor_version;
  phr_header headers[kMaxHeaders];
  size_t header_count = kMaxHeaders;

  int result = ::phr_parse_request(buffer_.c_str(), buffer_.size(),
                                   &method, &method_length,
                                   &path, &path_length, &minor_version,
                                   headers, &header_count, last_length);
  if (result > 0) {
    method_.assign(method, method_length);
    path_.assign(path, path_length);
    minor_version_ = minor_version;

    AddHeader(headers, header_count);

    buffer_.erase(0, result);
  }

  return result;
}
