// Copyright (c) 2013 dacci.org

#include "net/http/http_request.h"

#include <picohttpparser/picohttpparser.h>

HttpRequest::HttpRequest() {
}

HttpRequest::~HttpRequest() {
}

int HttpRequest::Parse(const char* data, size_t length) {
  const char* method;
  size_t method_length;
  const char* path;
  size_t path_length;
  int minor_version;
  phr_header headers[kMaxHeaders];
  size_t header_count = kMaxHeaders;

  int result = ::phr_parse_request(data, length, &method, &method_length,
                                   &path, &path_length, &minor_version,
                                   headers, &header_count, 0);
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
