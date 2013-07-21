// Copyright (c) 2013 dacci.org

#include "net/http/http_headers.h"

#include <string.h>

#include <picohttpparser/picohttpparser.h>

const std::string HttpHeaders::kNotFound;

HttpHeaders::HttpHeaders() {
}

HttpHeaders::~HttpHeaders() {
}

void HttpHeaders::AddHeader(const phr_header* headers, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    const phr_header& current = headers[i];
    std::string name(current.name, current.name_len);
    std::string value(current.value, current.value_len);

    for (size_t j = i + 1; j < count; ++j) {
      const phr_header& next = headers[j];
      if (next.name_len != 0)
        break;

      const char* start = next.value;
      const char* end = start + next.value_len;
      for (; start < end; ++start) {
        if (*start != '\t' && *start != ' ')
          break;
      }

      value.append(start, end);

      i = j;
    }

    AddHeader(std::move(name), std::move(value));
  }
}

void HttpHeaders::RemoveHeader(const std::string& name,
                               const std::string& value, bool exact) {
  typedef int (*Comparator)(const char*, const char*);
  Comparator comparator = exact ? strcmp : _stricmp;

  for (List::iterator i = list_.begin(), l = list_.end(); i != l; ++i) {
    if (::_stricmp(i->first.c_str(), name.c_str()) == 0 &&
        comparator(i->second.c_str(), value.c_str()) == 0) {
      list_.erase(i);
      break;
    }
  }
}

void HttpHeaders::RemoveHeader(const std::string& name) {
  List::iterator i = list_.begin();
  while (i != list_.end()) {
    if (::_stricmp(i->first.c_str(), name.c_str()) == 0) {
      if (i == list_.begin()) {
        list_.erase(i);
        i = list_.begin();
        continue;
      } else {
        List::iterator erase = i--;
        list_.erase(erase);
      }
    }

    ++i;
  }
}

bool HttpHeaders::HeaderExists(const std::string& name) {
  for (Iterator i = begin(), l = end(); i != l; ++i) {
    if (::_stricmp(i->first.c_str(), name.c_str()) == 0)
      return true;
  }

  return false;
}

const std::string& HttpHeaders::GetHeader(const std::string& name,
                                          size_t position) {
  size_t index = 0;

  for (Iterator i = begin(), l = end(); i != l; ++i) {
    if (::_stricmp(i->first.c_str(), name.c_str()) == 0) {
      if (index == position)
        return i->second;
      ++index;
    }
  }

  return kNotFound;
}

HttpHeaders::ValueList HttpHeaders::GetAllHeaders(const std::string& name) {
  ValueList list;

  for (Iterator i = begin(), l = end(); i != l; ++i) {
    if (::_stricmp(i->first.c_str(), name.c_str()) == 0)
      list.push_back(i->second);
  }

  return list;
}

void HttpHeaders::SerializeHeaders(std::string* buffer) {
  for (Iterator i = begin(), l = end(); i != l; ++i) {
    buffer->append(i->first);
    buffer->append(": ");
    buffer->append(i->second);
    buffer->append("\x0D\x0A");
  }
}
