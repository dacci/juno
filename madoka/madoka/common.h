// Copyright (c) 2013 dacci.org

#ifndef MADOKA_COMMON_H_
#define MADOKA_COMMON_H_

#ifndef DISALLOW_COPY_AND_ASSIGN
// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)
#endif  // DISALLOW_COPY_AND_ASSIGN

#endif  // MADOKA_COMMON_H_
