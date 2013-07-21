// Copyright (c) 2013 dacci.org

#ifndef JUNO_NET_HTTP_HTTP_HEADERS_H_
#define JUNO_NET_HTTP_HTTP_HEADERS_H_

#include <string.h>

#include <list>
#include <string>
#include <utility>

struct phr_header;

class HttpHeaders {
 public:
  typedef std::pair<std::string, std::string> Pair;
  typedef std::list<Pair> List;
  typedef List::const_iterator Iterator;
  typedef std::list<std::string> ValueList;

  static const std::string kNotFound;

  HttpHeaders();
  virtual ~HttpHeaders();

  void AddHeader(const std::string& name, const std::string& value) {
    list_.push_back(std::make_pair(name, value));
  }

  void AddHeader(const std::string& name, std::string&& value) {
    list_.push_back(std::make_pair(name, std::move(value)));
  }

  void AddHeader(std::string&& name, const std::string& value) {
    list_.push_back(std::make_pair(std::move(name), value));
  }

  void AddHeader(std::string&& name, std::string&& value) {
    list_.push_back(std::make_pair(std::move(name), std::move(value)));
  }

  void AddHeader(const phr_header* headers, size_t count);

  void RemoveHeader(const std::string& name, const std::string& value,
                    bool exact);
  void RemoveHeader(const std::string& name, const std::string& value) {
    RemoveHeader(name, value, false);
  }

  void RemoveHeader(const std::string& name);

  void ClearHeaders() {
    list_.clear();
  }

  bool HeaderExists(const std::string& name);

  const std::string& GetHeader(const std::string& name, size_t position);
  const std::string& GetHeader(const std::string& name) {
    return GetHeader(name, 0);
  }

  ValueList GetAllHeaders(const std::string& name);

  void SerializeHeaders(std::string* buffer);

  Iterator begin() const {
    return list_.begin();
  }

  Iterator end() const {
    return list_.end();
  }

 private:
  List list_;
};

#endif  // JUNO_NET_HTTP_HTTP_HEADERS_H_