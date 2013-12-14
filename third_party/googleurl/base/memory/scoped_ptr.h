// Copyright (c) 2013 dacci.org

#ifndef BASE_MEMORY_SCOPED_PTR_H_
#define BASE_MEMORY_SCOPED_PTR_H_

#include <memory>

template<typename Type>
class scoped_ptr : public std::unique_ptr<Type> {
};

#endif  // BASE_MEMORY_SCOPED_PTR_H_
