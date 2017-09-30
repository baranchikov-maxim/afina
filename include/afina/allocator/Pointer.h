#ifndef AFINA_ALLOCATOR_POINTER_H
#define AFINA_ALLOCATOR_POINTER_H

#include <afina/allocator/Simple.h>

namespace Afina {
namespace Allocator {
// Forward declaration. Do not include real class definition
// to avoid expensive macros calculations and increase compile speed
//class Simple;

class Pointer {
public:
    Pointer():
        _allocator(nullptr), _descriptor_num(0),  _mem_size(0) {}
    Pointer(Simple *allocator, size_t descriptor, size_t mem_size):
        _allocator(allocator), _descriptor_num(descriptor), _mem_size(mem_size) {}

    Pointer(const Pointer &) = default;
    Pointer(Pointer &&) = default;

    Pointer &operator=(const Pointer &) = default;
    Pointer &operator=(Pointer &&) = default;

    void *get() const {return _allocator != nullptr ? _allocator->get(_descriptor_num) : nullptr;}

    size_t mem_size() const noexcept {return _mem_size;}
    size_t descriptor() const noexcept {return _descriptor_num;}
    Simple *allocator() const noexcept {return _allocator;}

private:
    Simple *_allocator;
    size_t _descriptor_num;
    size_t _mem_size;
};

} // namespace Allocator
} // namespace Afina

#endif // AFINA_ALLOCATOR_POINTER_H
