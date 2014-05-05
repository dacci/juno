// Copyright (c) 2014 dacci.org

#ifndef JUNO_NET_HTTP_HTTP_UTIL_H_
#define JUNO_NET_HTTP_HTTP_UTIL_H_

#include <stdint.h>

#include <map>
#include <string>

namespace http {

// returns the size of chunk-size + chunk-data if successful,
// -2 if the chunk is partial or -1 on error.
int64_t ParseChunk(const std::string& buffer, int64_t* chunk_size);

}  // namespace http

#endif  // JUNO_NET_HTTP_HTTP_UTIL_H_
