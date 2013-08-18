// Copyright (c) 2013 dacci.org

#include "net/http/http_headers.h"

#include <string.h>

#include <boost/regex.hpp>

#include <picohttpparser/picohttpparser.h>

#include <utility>

const std::string HttpHeaders::kNotFound;

HttpHeaders::HttpHeaders() {
}

HttpHeaders::~HttpHeaders() {
}

void HttpHeaders::AppendHeader(const std::string& name,
                               const std::string& value) {
  for (auto i = list_.begin(), l = list_.end(); i != l; ++i) {
    if (::_stricmp(i->first.c_str(), name.c_str()) == 0) {
      i->second.append(", ");
      i->second.append(value);
      return;
    }
  }

  AddHeader(name, value);
}

void HttpHeaders::SetHeader(const std::string& name, const std::string& value) {
  bool first = true;
  bool found = false;

  for (auto i = list_.begin(), l = list_.end(); i != l; ++i) {
    found = true;

    if (::_stricmp(i->first.c_str(), name.c_str()) == 0) {
      if (first) {
        first = false;
        i->second = value;
      } else {
        list_.erase(i--);
      }
    }
  }

  if (!found)
    AddHeader(name, value);
}

void HttpHeaders::MergeHeader(const std::string& name,
                              const std::string& value) {
  // TODO(dacci)
  assert(false);
}

void HttpHeaders::EditHeader(const std::string& name, const std::string& value,
                             const std::string& replace, bool all) {
  boost::regex pattern;
  try {
    pattern.assign(value);
  } catch (...) {  // NOLINT(*)
    return;
  }

  boost::regex_constants::match_flag_type flags =
      boost::regex_constants::match_default;
  if (!all)
    flags |= boost::regex_constants::format_first_only;

  for (auto i = list_.begin(), l = list_.end(); i != l; ++i) {
    if (::_stricmp(i->first.c_str(), name.c_str()) == 0) {
      i->second = boost::regex_replace(i->second, pattern, replace, flags);
      if (!all)
        break;
    }
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
        list_.erase(i--);
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

void HttpHeaders::AddHeaders(const phr_header* headers, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    const phr_header& current = headers[i];
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

    list_.push_back(std::make_pair(std::string(current.name, current.name_len),
                                   std::move(value)));
  }
}
