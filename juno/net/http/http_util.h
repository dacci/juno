// Copyright (c) 2014 dacci.org

#ifndef JUNO_NET_HTTP_HTTP_UTIL_H_
#define JUNO_NET_HTTP_HTTP_UTIL_H_

#include <stdint.h>

#include <map>
#include <string>

namespace http {

struct less_ci {
  bool operator()(const std::string& left, const std::string& right) const {
    return ::_stricmp(left.c_str(), right.c_str()) < 0;
  }
};

typedef std::map<std::string, std::string, less_ci> AuthParamMap;

int64_t ParseChunk(const std::string& buffer, int64_t* chunk_size);
bool ParseAuthParams(const std::string& string, AuthParamMap* map);

}  // namespace http

#endif  // JUNO_NET_HTTP_HTTP_UTIL_H_
