#pragma once

#include <stdexcept>

namespace Afina {
namespace Allocator{

/**
 * Meta class for memory chunks
 * it will be always constructed on allocated memory =>
 * all assignments and copy constructors are forbidden
 *
 * All functions were included into header since I cannot add new *.cpp files into project
 */
class Block {
public:
    Block(const Block &) = delete;
    Block &operator=(const Block &) = delete;
    Block(Block &&) = delete;
    Block &operator=(Block &&) = delete;

    enum {META_SIZE = sizeof(size_t)};
    enum {MIN_MEM_SIZE = sizeof(void *), MIN_SIZE = MIN_MEM_SIZE + META_SIZE};

    // size of memory chunk and meta info
    explicit Block(size_t size)  {
        if(size < MIN_SIZE) {
            throw std::invalid_argument("wrong block size: too small");
        }
        _size = size;
    }

    size_t mem_size() const noexcept {return _size - META_SIZE;}
    static size_t meta_size() noexcept {return META_SIZE;}
    size_t size() const noexcept {return _size;}
    void resize(size_t size) {
        if (size < MIN_SIZE) {
            throw std::invalid_argument("wrong block size: too small");
        }
        _size = size;
    }

    // returns pointer to controlled memory chunk
    void *get() {return static_cast<void *>(this + 1);}

    //  returns pointer to next free block if this is free else garbage
    Block *next_free() { return *reinterpret_cast<Block **>(get());}
    void set_next_free(Block *block_ptr) { *reinterpret_cast<Block **>(get()) = block_ptr; }

    // returns pointer to next block or to descriptors table if this is last block
    Block *next() {
        return reinterpret_cast<Block *> (
            reinterpret_cast<uint8_t *>(this) + _size
        );
    }

    // constructs two new blocks from this
    // returns pointer to first one
    Block *cut_block(size_t size, bool was_free=true) {
        if (size > this->size()) {
            throw std::invalid_argument("cannot cut block: requested size bigger than this block");
        } else if (this->size() - size < MIN_SIZE) {
            throw std::invalid_argument("cannot cut block: size - requested size < minimal size");
        }

        size_t old_size = _size;
        this->resize(size);

        Block *new_block = new(this->next()) Block(old_size - size);

        if (was_free) {
            new_block->set_next_free(this->next_free());
            this->set_next_free(new_block);
        }

        return this;
    }

private:
    // size of raw memory chunk
    size_t _size;
};


} // namespace Allocator
} // namespace Afina
