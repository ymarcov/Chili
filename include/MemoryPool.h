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
private:
    static const std::size_t SlotSize = sizeof(T);

    MemoryPool(std::size_t pages) :
        _pages(pages),
        _buffer(CreateBuffer()),
        _head(_buffer),
        _freeSlots(GetCapacity()) {}

public:
    static std::shared_ptr<MemoryPool> Create(std::size_t pages = 1) {
        return std::shared_ptr<MemoryPool>{new MemoryPool{pages}};
    }

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
        Slot* currentHead = _head;

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

        auto correspondingSlot = static_cast<Slot*>(mem);

        Slot* currentHead = _head;
        correspondingSlot->_next = currentHead;

        while (!_head.compare_exchange_weak(currentHead, correspondingSlot)) {
            // we should be setting up the corresponding slot
            // as the new _head. keep trying till we get it right.
            correspondingSlot->_next = currentHead = _head;
        }

        // deallocation successful
        ++_freeSlots;
    }

    std::size_t GetCapacity() const noexcept {
        return GetBufferSize() / sizeof(Slot);
    }

    std::size_t GetFreeSlots() const {
        return _freeSlots;
    }

    void* GetBuffer() const noexcept {
        return _buffer;
    }

    std::size_t GetBufferSize() const noexcept {
        return _pages * ::getpagesize();
    }

private:
    union Slot {
        char _data[SlotSize];
        Slot* _next;
    };

    static bool IsPageAligned(void* mem) noexcept {
        return 0 == reinterpret_cast<std::uintptr_t>(mem) % ::getpagesize();
    }

    Slot* CreateBuffer() {
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

        auto slot = static_cast<Slot*>(mem);
        auto memEnd = static_cast<char*>(mem) + (::getpagesize() * _pages);
        auto lastSlot = reinterpret_cast<Slot*>(memEnd - sizeof(Slot));

        while (slot != lastSlot) {
            slot->_next = slot + 1;
            ++slot;
        }

        lastSlot->_next = nullptr;

        return static_cast<Slot*>(mem);
    }

    std::size_t _pages;
    Slot* _buffer;
    std::atomic<Slot*> _head;
    std::atomic_size_t _freeSlots;
};

} // namespace Http
} // namespace Yam

