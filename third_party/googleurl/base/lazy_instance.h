// Copyright (c) 2013 dacci.org

#ifndef BASE_LAZY_INSTANCE_H_
#define BASE_LAZY_INSTANCE_H_

#include <windows.h>

#include <memory>

namespace base {

#define LAZY_INSTANCE_INITIALIZER {SRWLOCK_INIT}

template<typename Type>
class LazyInstance {
 public:
  typedef LazyInstance<Type> Leaky;

  Type& Get() {
    return *Pointer();
  }

  Type* Pointer() {
    ::AcquireSRWLockExclusive(&lock_);

    if (!pointer_)
      pointer_.reset(new Type);

    ::ReleaseSRWLockExclusive(&lock_);

    return pointer_.get();
  }

  SRWLOCK lock_;
  std::unique_ptr<Type> pointer_;
};

}  // namespace base

#endif  // BASE_LAZY_INSTANCE_H_
