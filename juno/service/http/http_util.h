// Copyright (c) 2014 dacci.org

#ifndef JUNO_SERVICE_HTTP_HTTP_UTIL_H_
#define JUNO_SERVICE_HTTP_HTTP_UTIL_H_

#include <stdint.h>

#include <string>

#include "service/http/http_headers.h"

namespace juno {
namespace service {
namespace http {

extern const std::string kConnection;
extern const std::string kContentLength;
extern const std::string kExpect;
extern const std::string kKeepAlive;
extern const std::string kProxyAuthenticate;
extern const std::string kProxyAuthorization;
extern const std::string kProxyConnection;
extern const std::string kTransferEncoding;

namespace http_util {

// returns the value of Content-Length header if any,
// -2 if the message is chunked or -1 if unknown.
int64_t GetContentLength(const HttpHeaders& headers);

// returns the size of chunk-size + chunk-data if successful,
// -2 if the chunk is partial or -1 on error.
int64_t ParseChunk(const std::string& buffer, int64_t* chunk_size);

bool ProcessHopByHopHeaders(HttpHeaders* headers);

}  // namespace http_util
}  // namespace http
}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_HTTP_HTTP_UTIL_H_
