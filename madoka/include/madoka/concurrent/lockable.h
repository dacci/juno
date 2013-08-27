// Copyright (c) 2013 dacci.org

#ifndef MADOKA_CONCURRENT_LOCKABLE_H_
#define MADOKA_CONCURRENT_LOCKABLE_H_

namespace madoka {
namespace concurrent {

class Lockable {
 public:
  virtual ~Lockable() {}

  virtual void Lock() = 0;
  virtual void Unlock() = 0;
};

}  // namespace concurrent
}  // namespace madoka

#endif  // MADOKA_CONCURRENT_LOCKABLE_H_
