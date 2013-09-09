// Copyright (c) 2013 dacci.org

#ifndef MADOKA_CONCURRENT_LOCK_GUARD_H_
#define MADOKA_CONCURRENT_LOCK_GUARD_H_

#include <madoka/common.h>
#include <madoka/concurrent/lockable.h>

namespace madoka {
namespace concurrent {

class LockGuard {
 public:
  explicit LockGuard(Lockable* lock) : lock_(lock) {
    lock_->Lock();
  }

  ~LockGuard() {
    lock_->Unlock();
  }

 private:
  Lockable* const lock_;

  DISALLOW_COPY_AND_ASSIGN(LockGuard);
};

}  // namespace concurrent
}  // namespace madoka

#endif  // MADOKA_CONCURRENT_LOCK_GUARD_H_
