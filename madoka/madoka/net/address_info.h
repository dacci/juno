// Copyright (c) 2013 dacci.org

#ifndef MADOKA_NET_ADDRESS_INFO_H_
#define MADOKA_NET_ADDRESS_INFO_H_

#include <madoka/net/common.h>

#include <iterator>

namespace madoka {
namespace net {

template<typename InfoType>
class AddressInfoT : public InfoType {
 public:
  class iterator : public std::iterator<std::forward_iterator_tag, InfoType*> {
   public:
    bool operator==(const iterator& other) const {
      return current_ == other.current_;
    }

    bool operator!=(const iterator& other) const {
      return current_ != other.current_;
    }

    iterator& operator++() {
      current_ = current_->ai_next;
      return *this;
    }

    InfoType* operator*() const {
      return current_;
    }

   private:
    friend class AddressInfoT;

    explicit iterator(InfoType* start) : current_(start) {
    }

    InfoType* current_;
  };

  iterator begin() const {
    return iterator(entries_);
  }

  iterator end() const {
    return iterator(NULL);
  }

 protected:
  AddressInfoT() : InfoType(), entries_() {
  }

  InfoType* entries_;
  int error_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AddressInfoT);
};

class AddressInfo : public AddressInfoT<addrinfo> {
 public:
  AddressInfo() {
  }

  AddressInfo(const char* node_name, const char* service) {
    Resolve(node_name, service);
  }

  ~AddressInfo() {
    Free();
  }

  bool Resolve(const char* node_name, const char* service) {
    Free();

    error_ = ::getaddrinfo(node_name, service, this, &entries_);
    if (error_ != 0)
      return false;

    return true;
  }

  void Free() {
    if (entries_) {
      ::freeaddrinfo(entries_);
      entries_ = NULL;
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AddressInfo);
};

#ifdef WIN32

class AddressInfoW : public AddressInfoT<ADDRINFOW> {
 public:
  AddressInfoW() {
  }

  AddressInfoW(const wchar_t* node_name, const wchar_t* service) {
    Resolve(node_name, service);
  }

  ~AddressInfoW() {
    Free();
  }

  bool Resolve(const wchar_t* node_name, const wchar_t* service) {
    Free();

    error_ = ::GetAddrInfoW(node_name, service, this, &entries_);
    if (error_ != 0)
      return false;

    return true;
  }

  void Free() {
    if (entries_) {
      ::FreeAddrInfoW(entries_);
      entries_ = NULL;
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AddressInfoW);
};

#endif  // WIN32

}  // namespace net
}  // namespace madoka

#endif  // MADOKA_NET_ADDRESS_INFO_H_
