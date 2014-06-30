// Copyright (c) 2014 dacci.org

#include "net/http/http_util.h"

#include <stdlib.h>

namespace http_util {

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

}  // namespace http_util
