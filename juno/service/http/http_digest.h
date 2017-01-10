// Copyright (c) 2014 dacci.org

#ifndef JUNO_SERVICE_HTTP_HTTP_DIGEST_H_
#define JUNO_SERVICE_HTTP_HTTP_DIGEST_H_

#include <string>

namespace juno {
namespace service {
namespace http {

class HttpDigest {
 public:
  HttpDigest();

  bool Input(const std::string& input);
  bool Output(const std::string& method, const std::string& path,
              std::string* output);

  void SetCredential(const std::string& username, const std::string& password);

 private:
  enum class Algorithm;

  void Reset();

  std::string username_;
  std::string password_;

  int nonce_count_;
  Algorithm digest_;
  bool stale_;

  std::string nonce_;
  std::string cnonce_;
  std::string realm_;
  std::string opaque_;
  std::string qop_;
  std::string algorithm_;

  HttpDigest(const HttpDigest&) = delete;
  HttpDigest& operator=(const HttpDigest&) = delete;
};

}  // namespace http
}  // namespace service
}  // namespace juno

#endif  // JUNO_SERVICE_HTTP_HTTP_DIGEST_H_
