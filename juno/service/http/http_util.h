// Copyright (c) 2014 dacci.org

#ifndef JUNO_SERVICE_HTTP_HTTP_UTIL_H_
#define JUNO_SERVICE_HTTP_HTTP_UTIL_H_

#include <stdint.h>

#include <map>
#include <string>

#include "service/http/http_headers.h"

namespace http_util {

// returns the value of Content-Length header if any,
// -2 if the message is chunked or -1 if unknown.
int64_t GetContentLength(const HttpHeaders& headers);

// returns the size of chunk-size + chunk-data if successful,
// -2 if the chunk is partial or -1 on error.
int64_t ParseChunk(const std::string& buffer, int64_t* chunk_size);

bool ProcessHopByHopHeaders(HttpHeaders* headers);

}  // namespace http_util

#endif  // JUNO_SERVICE_HTTP_HTTP_UTIL_H_
