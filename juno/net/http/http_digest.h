// Copyright (c) 2014 dacci.org

#ifndef JUNO_NET_HTTP_HTTP_DIGEST_H_
#define JUNO_NET_HTTP_HTTP_DIGEST_H_

#include <string>

class HttpDigest {
 public:
  HttpDigest(const std::string& username, const std::string& password);

  bool Input(const std::string& input);
  bool Output(const std::string& method, const std::string& path,
              std::string* output);

  void SetCredential(const std::string& username, const std::string& password);

 private:
  enum Algorithm {
    MD5SESS, MD5
  };

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
};

#endif  // JUNO_NET_HTTP_HTTP_DIGEST_H_
