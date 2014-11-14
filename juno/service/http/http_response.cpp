// Copyright (c) 2013 dacci.org

#include "service/http/http_response.h"

#include <picohttpparser/picohttpparser.h>

#include <string>

HttpResponse::HttpResponse() {
}

HttpResponse::~HttpResponse() {
}

int HttpResponse::Parse(const char* data, size_t length) {
  int minor_version;
  int status;
  const char* message;
  size_t message_length;
  phr_header headers[kMaxHeaders];
  size_t header_count = kMaxHeaders;

  int result = ::phr_parse_response(data, length, &minor_version, &status,
                                    &message, &message_length,
                                    headers, &header_count, 0);
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
  ::sprintf_s(status, "HTTP/1.%d %d ", minor_version_, status_);

  output->append(status).append(message_).append("\x0D\x0A");
  SerializeHeaders(output);
  output->append("\x0D\x0A");
}
