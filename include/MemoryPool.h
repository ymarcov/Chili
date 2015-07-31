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
        if (!t) throw std::bad_alloc();
        try { new(t) T{std::forward<Args>(args)...}; }
        catch(...) { Deallocate(t); throw; }
        auto deleter = [this](T* t) { Delete(t); };
        return std::unique_ptr<T, decltype(deleter)>(t, deleter);
    }

    void Delete(T* t) {
        t->~T();
        Deallocate(t);
    }

    void* Allocate() {
        Chunk* mem = _head;
        if (!mem) return nullptr;
        while (!_head.compare_exchange_weak(mem, mem->_next))
            if (!(mem = _head)) return nullptr;
        --_freeSlots;
        return mem;
    }

    void Deallocate(void* mem) {
        if (!mem) return;
        auto c = static_cast<Chunk*>(mem);
        Chunk* head = _head;
        c->_next = head;
        while (!_head.compare_exchange_weak(head, c))
            c->_next = head = _head;
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

    bool IsPageAligned(void* mem) {
        return 0 == reinterpret_cast<std::uintptr_t>(mem) % ::getpagesize();
    }

    Chunk* CreateBuffer() {
        auto mem = ::mmap(nullptr,
                GetBufferSize(),
                PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS,
                0, 0);

        if (mem == MAP_FAILED)
            throw SystemError();

        if (!IsPageAligned(mem))
            throw std::logic_error("mmap() didn't return page-aligned memory");

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

