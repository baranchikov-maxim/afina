#ifndef AFINA_ALLOCATOR_SIMPLE_H
#define AFINA_ALLOCATOR_SIMPLE_H

#include <string>
#include <cstddef>

namespace Afina {
namespace Allocator {

class Block;

// Forward declaration. Do not include real class definition
// to avoid expensive macros calculations and increase compile speed
class Pointer;

/**
 * Wraps given memory area and provides defagmentation allocator interface on
 * the top of it.
 *
 * Allocator instance doesn't take ownership of wrapped memmory and do not delete it
 * on destruction. So caller must take care of resource cleaup after allocator stop
 * being needs
 */
// TODO: Implements interface to allow usage as C++ allocators
class Simple {
public:
    enum {DESCRIPTORS_INIT = 400};
    enum {DESCRIPTORS_INC = 30};

    Simple(void *base, const size_t size);

    /**
     * TODO: semantics
     * @param N size_t
     */
    Pointer alloc(size_t N);

    /**
     * TODO: semantics
     * @param p Pointer
     * @param N size_t
     */
    void realloc(Pointer &p, size_t N);

    /**
     * TODO: semantics
     * @param p Pointer
     */
    void free(Pointer &p);

    /**
     * TODO: semantics
     */
    void defrag();

    /**
     * TODO: semantics
     */
    std::string dump() const;

    void *get(size_t descriptor_num) const;

private:
    uint8_t *_base;
    const size_t _base_len;


    enum SHIFT {FREE_BLOCK = sizeof(Block *), DESC_LEN = FREE_BLOCK + sizeof(size_t), DESC = DESC_LEN + sizeof(size_t)};

    // Helpful macro
    uint8_t *_shl(size_t shift) const { return _base + _base_len - shift; }

    Block *&_first_free_block() { return *reinterpret_cast<Block **>(_shl(FREE_BLOCK)); }
    Block *_last_free_block();
    // doesnt work with defrag
    bool _block_is_free(Block *);

    size_t &_descriptors_len() { return *reinterpret_cast<size_t *>(_shl(DESC_LEN)); }
    size_t _descriptors_len() const { return *reinterpret_cast<size_t *>(_shl(DESC_LEN)); }

    size_t &_descriptor(size_t i) { return *(reinterpret_cast<size_t *>(_shl(DESC)) - i); }
    size_t _descriptor(size_t i) const { return *(reinterpret_cast<size_t *>(_shl(DESC)) - i); }
    size_t *_block_to_descriptor_ptr(Block *);

    bool _descriptor_is_free(size_t descriptor_num) const;
    // occupies free descriptor if there is free else tries to extend descriptors table
    size_t _get_free_descriptor();
    void _extend_descriptors(size_t inc=DESCRIPTORS_INC);
};

} // namespace Allocator
} // namespace Afina
#endif // AFINA_ALLOCATOR_SIMPLE_H
