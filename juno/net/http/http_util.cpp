// Copyright (c) 2014 dacci.org

#include "net/http/http_util.h"

#include <utility>

namespace http {

int64_t ParseChunk(const std::string& buffer, int64_t* chunk_size) {
  char* start = const_cast<char*>(buffer.c_str());
  char* end = start;
  *chunk_size = ::_strtoi64(start, &end, 16);

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

// derived from cURL's http_digest.c
bool ParseAuthParams(const std::string& string, AuthParamMap* map) {
  bool starts_with_quote = false;
  bool escape = false;
  std::string key, value;
  auto end = string.end();
  auto left = string.begin();

  while (left != end && ::isspace(*left))
    ++left;

  auto right = left;
  while (right != end && !::isspace(*right))
    ++right;

  key.assign(left, right);
  map->insert(std::make_pair("__scheme__", std::move(key)));

  left = right;

  while (left != end) {
    while (left != end && ::isspace(*left))
      ++left;

    for (right = left; right != end && *right != '=';)
      ++right;

    key.assign(left, right);

    if (right == end || *right != '=')
      // eek, no match
      return false;

    left = ++right;

    if (*left == '\"') {
      // this starts with a quote so it must end with one as well!
      ++left;
      starts_with_quote = true;
    }

    bool done = false;
    for (right = left; right != end && !done; right++) {
      switch (*right) {
        case '\\':
          if (!escape) {
            // possibly the start of an escaped quote
            escape = true;
            // even though this is an escape character,
            // we still store it as-is in the target buffer
            value.push_back('\\');
            continue;
          }
          break;

        case ',':
          if (!starts_with_quote) {
            // this signals the end of the content if we didn't get
            // a starting quote and then we do "sloppy" parsing
            done = true;
            continue;
          }
          break;

        case '\x0D':
        case '\x0A':
          // end of string
          done = true;
          continue;

        case '\"':
          if (!escape && starts_with_quote) {
            // end of string
            done = true;
            continue;
          }
          break;
      }

      escape = false;
      value.push_back(*right);
    }

    map->insert(std::make_pair(std::move(key), std::move(value)));

    // pass all additional spaces here
    while (right != end && ::isspace(*right))
      ++right;

    if (right != end && *right == ',')
      // allow the list to be comma-separated
      ++right;

    left = right;
  }

  return true;  // all is fine!
}

}  // namespace http
