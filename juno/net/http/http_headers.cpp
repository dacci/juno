// Copyright (c) 2013 dacci.org

#include "net/http/http_headers.h"

#include <string.h>

#include <picohttpparser/picohttpparser.h>

#include <regex>
#include <utility>

const std::string HttpHeaders::kNotFound;

HttpHeaders::HttpHeaders() {
}

HttpHeaders::~HttpHeaders() {
}

void HttpHeaders::AppendHeader(const std::string& name,
                               const std::string& value) {
  for (auto& pair : list_) {
    if (::_stricmp(pair.first.c_str(), name.c_str()) == 0) {
      pair.second.append(", ");
      pair.second.append(value);
      return;
    }
  }

  AddHeader(name, value);
}

void HttpHeaders::SetHeader(const std::string& name, const std::string& value) {
  bool first = true;
  bool found = false;

  for (auto i = list_.begin(), l = list_.end(); i != l;) {
    if (::_stricmp(i->first.c_str(), name.c_str()) == 0) {
      found = true;

      if (first) {
        first = false;
        i->second = value;
      } else {
        list_.erase(i++);
        continue;
      }
    }

    ++i;
  }

  if (!found)
    AddHeader(name, value);
}

void HttpHeaders::MergeHeader(const std::string& name,
                              const std::string& value) {
  for (auto& pair : list_) {
    if (::_stricmp(pair.first.c_str(), name.c_str()) == 0) {
      const char* start = pair.second.c_str();
      const char* end = start;

      while (true) {
        while (*end) {
          if (*end == ',')
            break;

          ++end;
        }

        if (::_strnicmp(value.c_str(), start, end - start) == 0)
          return;

        if (*end == '\0')
          break;

        start = end + 1;

        while (*start) {
          if (*start != ' ')
            break;

          ++start;
        }

        if (*start == '\0')
          break;

        end = start;
      }

      pair.second.append(", ");
      pair.second.append(value);
      return;
    }
  }

  AddHeader(name, value);
}

void HttpHeaders::EditHeader(const std::string& name, const std::string& value,
                             const std::string& replace, bool all) {
  std::regex pattern;
  try {
    pattern.assign(value);
  } catch (...) {  // NOLINT(*)
    return;
  }

  std::regex_constants::match_flag_type flags =
      std::regex_constants::match_default;
  if (!all)
    flags |= std::regex_constants::format_first_only;

  for (auto& pair : list_) {
    if (::_stricmp(pair.first.c_str(), name.c_str()) == 0) {
      pair.second = std::regex_replace(pair.second, pattern, replace, flags);
      if (!all)
        break;
    }
  }
}

void HttpHeaders::RemoveHeader(const std::string& name,
                               const std::string& value, bool exact) {
  typedef int (*Comparator)(const char*, const char*);
  Comparator comparator = exact ? strcmp : _stricmp;

  for (auto i = list_.begin(), l = list_.end(); i != l; ++i) {
    if (::_stricmp(i->first.c_str(), name.c_str()) == 0 &&
        comparator(i->second.c_str(), value.c_str()) == 0) {
      list_.erase(i);
      break;
    }
  }
}

void HttpHeaders::RemoveHeader(const std::string& name) {
  for (auto i = list_.begin(), l = list_.end(); i != l;) {
    if (::_stricmp(i->first.c_str(), name.c_str()) == 0)
      list_.erase(i++);
    else
      ++i;
  }
}

bool HttpHeaders::HeaderExists(const std::string& name) {
  for (auto& pair : *this) {
    if (::_stricmp(pair.first.c_str(), name.c_str()) == 0)
      return true;
  }

  return false;
}

const std::string& HttpHeaders::GetHeader(const std::string& name,
                                          size_t position) {
  size_t index = 0;

  for (auto& pair : *this) {
    if (::_stricmp(pair.first.c_str(), name.c_str()) == 0) {
      if (index == position)
        return pair.second;
      ++index;
    }
  }

  return kNotFound;
}

HttpHeaders::ValueList HttpHeaders::GetAllHeaders(const std::string& name) {
  ValueList list;

  for (auto& pair : *this) {
    if (::_stricmp(pair.first.c_str(), name.c_str()) == 0)
      list.push_back(pair.second);
  }

  return list;
}

void HttpHeaders::SerializeHeaders(std::string* buffer) {
  for (auto& pair : *this) {
    buffer->append(pair.first);
    buffer->append(": ");
    buffer->append(pair.second);
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
