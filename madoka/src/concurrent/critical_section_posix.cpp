// Copyright (c) 2013 dacci.org

#include <madoka/concurrent/critical_section.h>

#include <pthread.h>

namespace madoka {
namespace concurrent {

struct CriticalSection::MutexImpl {
  pthread_mutex_t mutex;
};

CriticalSection::CriticalSection() : mutex_(new MutexImpl) {
  mutex_->mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
}

CriticalSection::~CriticalSection() {
  ::pthread_mutex_destroy(&mutex_->mutex);
  delete mutex_;
}

void CriticalSection::Lock() {
  ::pthread_mutex_lock(&mutex_->mutex);
}

bool CriticalSection::TryLock() {
  return ::pthread_mutex_trylock(&mutex_->mutex) == 0;
}

void CriticalSection::Unlock() {
  ::pthread_mutex_unlock(&mutex_->mutex);
}

}  // namespace concurrent
}  // namespace madoka
