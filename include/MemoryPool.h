#pragma once

#include "SystemError.h"

#include <atomic>
#include <memory>
#include <new>
#include <stdexcept>
#include <sys/mman.h>
#include <unistd.h>

namespace Yam {
namespace Http {

template <class T>
class MemoryPool {
public:
    static const std::size_t ChunkSize = sizeof(T);

    MemoryPool(std::size_t pages = 1) :
        _pages(pages),
        _buffer(CreateBuffer()),
        _head(_buffer),
        _freeSlots(GetCapacity()) {}

    ~MemoryPool() {
        ::munmap(_buffer, GetBufferSize());
    }

    template <class... Args>
    auto New(Args&&... args) {
        auto t = static_cast<T*>(Allocate());

        if (!t)
            throw std::bad_alloc();

        try {
            new(t) T{std::forward<Args>(args)...};
        } catch(...) {
            Deallocate(t);
            throw;
        }

        auto deleter = [this](T* t) { Delete(t); };
        return std::unique_ptr<T, decltype(deleter)>(t, deleter);
    }

    void Delete(T* t) {
        t->~T();
        Deallocate(t);
    }

    void* Allocate() {
        Chunk* currentHead = _head;

        if (!currentHead) {
            // Oops: out of memory!
            return nullptr;
        }

        while (!_head.compare_exchange_weak(currentHead, currentHead->_next)) {
            // try to replace _head (assuming it still equals currentHead)
            // with the next one, thereby releasing its node for external use.
            if (!(currentHead = _head)) {
                // Oops: out of memory!
                return nullptr;
            }
        }

        // allocation successful
        --_freeSlots;

        return currentHead;
    }

    void Deallocate(void* mem) {
        if (!mem)
            return;

        auto correspondingChunk = static_cast<Chunk*>(mem);

        Chunk* currentHead = _head;
        correspondingChunk->_next = currentHead;

        while (!_head.compare_exchange_weak(currentHead, correspondingChunk)) {
            // we should be setting up the corresponding chunk
            // as the new _head. keep trying till we get it right.
            correspondingChunk->_next = currentHead = _head;
        }

        // deallocation successful
        ++_freeSlots;
    }

    std::size_t GetCapacity() const noexcept {
        return GetBufferSize() / sizeof(Chunk);
    }

    std::size_t GetFreeSlots() const {
        return _freeSlots;
    }

    void* GetBuffer() const {
        return _buffer;
    }

    std::size_t GetBufferSize() const {
        return _pages * ::getpagesize();
    }

private:
    union Chunk {
        char _data[ChunkSize];
        Chunk* _next;
    };

    static bool IsPageAligned(void* mem) {
        return 0 == reinterpret_cast<std::uintptr_t>(mem) % ::getpagesize();
    }

    Chunk* CreateBuffer() {
        // first thing, get all the memory we need
        // and make sure it meets all the expectations.

        auto mem = ::mmap(nullptr,
                GetBufferSize(),
                PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS,
                0, 0);

        if (mem == MAP_FAILED)
            throw SystemError();

        if (!IsPageAligned(mem))
            throw std::logic_error("mmap() didn't return page-aligned memory");

        // now set up the linked list. remember that we need
        // the last one to point to null as its next node,
        // signifying that there's no more memory following it.

        auto chunk = static_cast<Chunk*>(mem);
        auto memEnd = static_cast<char*>(mem) + (::getpagesize() * _pages);
        auto nextToLastChunk = reinterpret_cast<Chunk*>(memEnd - sizeof(Chunk));

        while (chunk != nextToLastChunk) {
            chunk->_next = chunk + 1;
            ++chunk;
        }

        chunk->_next = nullptr; // last chunk points to null

        return static_cast<Chunk*>(mem);
    }

    std::size_t _pages;
    Chunk* _buffer;
    std::atomic<Chunk*> _head;
    std::atomic_size_t _freeSlots;
};

} // namespace Http
} // namespace Yam

