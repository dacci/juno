// Copyright (c) 2014 dacci.org

#include "service/http/http_util.h"

#include <stdlib.h>

#include <string>

namespace {
const std::string kConnection("Connection");
const std::string kContentLength("Content-Length");
const std::string kKeepAlive("Keep-Alive");
const std::string kProxyAuthenticate("Proxy-Authenticate");
const std::string kProxyAuthorization("Proxy-Authorization");
const std::string kProxyConnection("Proxy-Connection");
const std::string kTransferEncoding("Transfer-Encoding");
}  // namespace

namespace http_util {

int64_t GetContentLength(const HttpHeaders& headers) {
  // RFC 2616 - 4.4 Message Length
  if (headers.HeaderExists(kTransferEncoding) &&
      _stricmp(headers.GetHeader(kTransferEncoding).c_str(), "identity") != 0)
    return -2;
  else if (headers.HeaderExists(kContentLength))
    return std::stoll(headers.GetHeader(kContentLength));
  else
    return -1;
}

int64_t ParseChunk(const std::string& buffer, int64_t* chunk_size) {
  char* start = const_cast<char*>(buffer.c_str());
  char* end = start;
  *chunk_size = ::_strtoi64(start, &end, 16);

  while (*end) {
    if (*end != '\x0D' && *end != '\x0A' && *end != ' ')
      return -1;

    if (*end == '\x0A')
      break;

    ++end;
  }

  if (*end == '\0')
    return -2;

  ++end;

  int64_t length = *chunk_size + (end - start);
  if (buffer.size() < length)
    return -2;

  end = start + length;

  while (*end) {
    if (*end != '\x0D' && *end != '\x0A')
      return -1;

    if (*end == '\x0A')
      break;

    ++end;
  }

  if (*end == '\0')
    return -2;

  ++end;

  return end - start;
}

bool ProcessHopByHopHeaders(HttpHeaders* headers) {
  bool has_close = false;

  if (headers->HeaderExists(kConnection)) {
    const std::string& connection = headers->GetHeader(kConnection);
    size_t start = 0;

    while (true) {
      size_t end = connection.find_first_of(',', start);
      size_t length = std::string::npos;
      if (end != std::string::npos)
        length = end - start;

      std::string token = connection.substr(start, length);
      if (::_stricmp(token.c_str(), "close") == 0)
        has_close = true;
      else
        headers->RemoveHeader(token);

      if (end == std::string::npos)
        break;

      start = connection.find_first_not_of(" \t", end + 1);
    }

    headers->RemoveHeader(kConnection);
  }

  // remove other hop-by-hop headers
  headers->RemoveHeader(kKeepAlive);
  headers->RemoveHeader(kProxyConnection);

  // According to RFC 2616, Proxy-Authenticate and Proxy-Authorization headers
  // are hop-by-hop headers. However, leave them untouched and process later.

  return has_close;
}

}  // namespace http_util
