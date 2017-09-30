#include <afina/allocator/Simple.h>
#include <afina/allocator/Pointer.h>
#include <afina/allocator/Block.h>
#include <afina/allocator/Error.h>

#include <cstring>

namespace Afina {
namespace Allocator {

Simple::Simple(void *base, size_t size):
    _base(static_cast<uint8_t *>(base)),
    _base_len(size)
{
    _descriptors_len() = DESCRIPTORS_INIT;
    // init list of free descriptors
    for (size_t i = 0; i < DESCRIPTORS_INIT; ++i) {
        _descriptor(i) = i + 1;
    }
    _descriptor(DESCRIPTORS_INIT - 1) = 0;

    size_t mem_size = size -
        sizeof(Block *) -  // pointer to first block
        sizeof(size_t) - // descriptors len
        DESCRIPTORS_INIT*sizeof(size_t); // descriptors

    _first_free_block() = new(_base) Block(mem_size);
    _first_free_block()->set_next_free(nullptr);
}

/**
 * TODO: semantics
 * @param N size_t
 */
Pointer Simple::alloc(size_t N) {
    size_t required_size = N < Block::MIN_MEM_SIZE ? Block::MIN_SIZE : N+Block::META_SIZE;
    // searching for block
    Block *bp = _first_free_block();
    if (bp == nullptr) {
        throw AllocError(AllocErrorType::NoMemory, "no free blocks");
    }
    while (bp->size() < required_size && bp->next_free() != nullptr) {
        bp = bp->next_free();
    }
    if (bp->size() < required_size)  { // defrag ?
        throw AllocError(AllocErrorType::NoMemory, "block of acceptable size wasn't found");
    }

    // if block was found with more than requested size
    if (bp->size() - required_size > Block::MIN_SIZE) {
        bp = bp->cut_block(required_size);
    }

    // if we are returning first free block
    if (bp == _first_free_block()) {
        _first_free_block() = bp->next_free();
    }

    // write offset into descriptor
    size_t descriptor_num = _get_free_descriptor();
    _descriptor(descriptor_num) = static_cast<uint8_t *>(bp->get()) - _base;

    return Pointer(this, descriptor_num, N < Block::MIN_MEM_SIZE ? Block::MIN_MEM_SIZE : N);
}

/**
 * TODO: semantics
 * @param p Pointer
 * @param N size_t
 */
void Simple::realloc(Pointer &p, size_t N) {
    if (N < Block::MIN_MEM_SIZE) {
        N = Block::MIN_MEM_SIZE;
    }

    if (p.get() == nullptr) {
        p = this->alloc(N);
        return;

    } else if (p.allocator() != this) {
        Pointer new_p = this->alloc(N);
        memcpy(new_p.get(), p.get(), std::min(N, p.mem_size()));
        p.allocator()->free(p);
        p = new_p;
        return;

    } else if (N <= p.mem_size()) {
        // not enough memory to create new block
        if (p.mem_size() - N < Block::MIN_SIZE) {
            return;
        }

        Block *bp = reinterpret_cast<Block *>(
            static_cast<uint8_t *>(p.get()) - Block::META_SIZE
        );
        // create new block
        bp = bp->cut_block(N + bp->meta_size(), /**was_free=**/false)->next();
        // append new block
        bp->set_next_free(_first_free_block());
        _first_free_block() = bp;
        p = Pointer(p.allocator(), p.descriptor(), N);
        return;

    } else if (N > p.mem_size()) {
        Block *bp = reinterpret_cast<Block *> (
            static_cast<uint8_t *>(p.get()) - Block::META_SIZE
        );
        // check if next block is free
        Block *b_it = _first_free_block();
        while (b_it != nullptr && b_it != bp->next() && b_it->next_free() != bp->next()) {
            b_it = b_it->next_free();
        }

        if (b_it == nullptr) {
            Pointer new_p = this->alloc(N);
            memcpy(new_p.get(), p.get(), p.mem_size());
            this->free(p);
            p = new_p;
            return;

        } else {
            if (_first_free_block() == bp->next()) {
                _first_free_block() = _first_free_block()->next_free();
            } else {
                b_it->set_next_free(b_it->next_free()->next_free());
            }

            if (bp->next()->size() - (N - p.mem_size()) >= Block::MIN_SIZE) {
                Block *new_block = bp->next()->cut_block(N - p.mem_size())->next();
                new_block->set_next_free(_first_free_block());
                _first_free_block() = new_block;

                bp->resize( bp->size() + (N - p.mem_size()) );
                p = Pointer(p.allocator(), p.descriptor(), N);
                return;

            } else {
                bp->resize( bp->size() + bp->next()->size() );
                p = Pointer(p.allocator(), p.descriptor(), p.mem_size() + bp->next()->size());
                return;
            }
        }
    }
}

/**
 * TODO: semantics
 * @param p Pointer
 */
void Simple::free(Pointer &p) {
    if (p.allocator() != this) {
        throw AllocError(AllocErrorType::InvalidFree, "wrong allocator");
    } else if (p.descriptor() == 0 || p.descriptor() >= _descriptors_len() || _descriptor_is_free(p.descriptor())) {
        throw AllocError(AllocErrorType::InvalidFree, "invalid descriptor");
    }

    // free block
    Block *bp = reinterpret_cast<Block *> (
        static_cast<uint8_t *>(p.get()) - Block::meta_size()
    );

    bp->set_next_free(_first_free_block());
    _first_free_block() = bp;

    // free descriptor
    size_t desc = p.descriptor();
    _descriptor(desc) = _descriptor(0);
    _descriptor(0) = desc;
}

/**
 * TODO: semantics
 */
void Simple::defrag() {
    Block *dst_b = _first_free_block();
    if (dst_b == nullptr) {
        return;
    }
    for (Block *bp = _first_free_block(); bp->next_free() != nullptr; bp = bp->next_free()) {
        if (bp < dst_b) {
            dst_b = bp;
        }
    }
    uint8_t *dst = reinterpret_cast<uint8_t *>(dst_b);

    Block *src_b = reinterpret_cast<Block *>(_base);
    while (src_b < dst_b) {
        src_b = src_b->next();
    }
    uint8_t *src = reinterpret_cast<uint8_t *>(src_b);

    size_t dst_last_size = 0;
    while (reinterpret_cast<size_t *>(src) < &_descriptor(_descriptors_len())) {
        Block *src_block = reinterpret_cast<Block *>(src);
        size_t src_size = src_block->size();

        size_t *desc_ptr = _block_to_descriptor_ptr(src_block);
        if (desc_ptr != nullptr) {
            memmove(dst, src, src_block->size());

            *desc_ptr = (dst + Block::META_SIZE) - _base;

            dst_last_size = src_size;
            dst += dst_last_size;
        }

        src += src_size;
    }

    uint8_t *new_block_addr = dst + dst_last_size;
    size_t new_block_size = reinterpret_cast<uint8_t *>(&_descriptor(_descriptors_len() - 1)) - new_block_addr;
    if (new_block_size >= Block::MIN_SIZE) {
        _first_free_block() = new(new_block_addr) Block(new_block_size);
        _first_free_block()->set_next_free(nullptr);
    } else {
        _first_free_block() = nullptr;
    }
}

/**
 * TODO: semantics
 */
std::string Simple::dump() const { return ""; }

void *Simple::get(size_t descriptor_num) const {
    if (descriptor_num == 0 || descriptor_num >= _descriptors_len() || _descriptor_is_free(descriptor_num) ) {
        return nullptr;
    }
    return _base + _descriptor(descriptor_num);
}

//------------------------------------------
//-------------- PRIVATE SECTION -----------
//------------------------------------------

bool Simple::_descriptor_is_free(size_t descriptor_num) const {
    size_t desc = _descriptor(0);
    while (desc != 0) {
        if (desc == descriptor_num) {
            return true;
        }
        desc = _descriptor(desc);
    }
    return false;
}

size_t Simple::_get_free_descriptor() {
    if (_descriptor(0) == 0) {
        _extend_descriptors();
    }

    size_t free_desc_num = _descriptor(0);
    _descriptor(0) = _descriptor(free_desc_num);

    return free_desc_num;
}

void Simple::_extend_descriptors(size_t inc/**=DESCRIPTORS_INC**/) {
    Block *bp = _last_free_block(); // victim Block to cut
    if (bp == nullptr) {
        throw AllocError(AllocErrorType::NoMemory, "no free blocks");
    }

    // assert bp pointers to block which neighboring with descriptors
    if (reinterpret_cast<uint8_t *>(bp)+bp->size() <
        reinterpret_cast<uint8_t *>(&_descriptor(_descriptors_len())) )
    {
        throw AllocError(AllocErrorType::NoMemory, "no memory to extend descriptors");
    }

    if (bp->mem_size() < Block::MIN_MEM_SIZE + inc*sizeof(size_t)) {
        throw AllocError(AllocErrorType::NoMemory, "no memory for descriptors");
    }
    bp->resize(bp->size() - inc*sizeof(size_t));

    // append new descriptors to list
    size_t first = _descriptor(0);
    _descriptor(0) = _descriptors_len();
    for (size_t i = 0; i < inc; ++i) {
        _descriptor(_descriptors_len() + i) = _descriptors_len() + i + 1;
    }
    _descriptors_len() += inc;
    _descriptor(_descriptors_len() - 1) = 0;
}

Block *Simple::_last_free_block() {
    Block *last = _first_free_block();
    if (last == nullptr) {
        return nullptr;
    }
    while (last->next_free() != nullptr) {
        last = last->next_free();
    }

    return last;
}

bool Simple::_block_is_free(Block *block_ptr) {
    for (Block *bp = _first_free_block(); bp != nullptr; bp = bp->next_free()) {
        if (bp == block_ptr) {
            return true;
        }
    }
    return false;
}

size_t *Simple::_block_to_descriptor_ptr(Block *bp) {
    size_t offset = static_cast<uint8_t *>(bp->get()) - _base;
    size_t *desc_base = &_descriptor(0);
    for (int i = 1; i < _descriptors_len(); ++i) {
        if ( desc_base[-i] == offset && !_descriptor_is_free(i) ) {
            return desc_base - i;
        }
    }
    return nullptr;
}


} // namespace Allocator
} // namespace Afina
