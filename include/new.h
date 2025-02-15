#ifndef NEW_H_INCLUDED
#define NEW_H_INCLUDED

#include <cstring>
#include <cstdint>
#include <cstddef>
#include <new>

using size_t = std::size_t;

// inline
// void *operator new( std::size_t, void *ptr) noexcept { return ptr; }

// inline
// void operator delete(void *, void *)noexcept {}

// inline
// void *operator new[]( std::size_t, void *ptr) noexcept { return ptr; }

// inline
// void operator delete[](void *, void *)noexcept {}

// void *operator new(std::size_t size);
// void operator delete(void *)noexcept;

// void *operator new[](std::size_t size);
// void operator delete[](void *)noexcept;

#endif // NEW_H_INCLUDED
