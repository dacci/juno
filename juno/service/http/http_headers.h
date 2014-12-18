// Copyright (c) 2013 dacci.org

#ifndef JUNO_SERVICE_HTTP_HTTP_HEADERS_H_
#define JUNO_SERVICE_HTTP_HTTP_HEADERS_H_

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

  // The response header is added to the existing set of headers,
  // even if this header already exists.
  void AddHeader(const std::string& name, const std::string& value) {
    list_.push_back(std::make_pair(name, value));
  }

  // The request header is appended to any existing header of the same name.
  // When a new value is merged onto an existing header it is separated
  // from the existing header with a comma. This is the HTTP standard way of
  // giving a header multiple values.
  void AppendHeader(const std::string& name, const std::string& value);

  // The response header is set, replacing any previous header with this name.
  void SetHeader(const std::string& name, const std::string& value);

  // The request header is appended to any existing header of the same name,
  // unless the value to be appended already appears in the existing header's
  // comma-delimited list of values. When a new value is merged onto
  // an existing header it is separated from the existing header with a comma.
  // This is the HTTP standard way of giving a header multiple values.
  // Values are compared in a case sensitive manner.
  void MergeHeader(const std::string& name, const std::string& value);

  // If this request header exists, its value is transformed according to
  // a regular expression search-and-replace.
  void EditHeader(const std::string& name, const std::string& find,
                  const std::string& replace, bool all);

  void RemoveHeader(const std::string& name, const std::string& value,
                    bool exact);
  void RemoveHeader(const std::string& name, const std::string& value) {
    RemoveHeader(name, value, false);
  }

  void RemoveHeader(const std::string& name);

  void ClearHeaders() {
    list_.clear();
  }

  bool HeaderExists(const std::string& name) const;

  const std::string& GetHeader(const std::string& name, size_t position) const;
  const std::string& GetHeader(const std::string& name) const {
    return GetHeader(name, 0);
  }

  ValueList GetAllHeaders(const std::string& name) const;

  void SerializeHeaders(std::string* buffer) const;

  Iterator begin() const {
    return list_.begin();
  }

  Iterator end() const {
    return list_.end();
  }

 protected:
  void AddHeaders(const phr_header* headers, size_t count);

 private:
  List list_;
};

#endif  // JUNO_SERVICE_HTTP_HTTP_HEADERS_H_
