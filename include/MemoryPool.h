#pragma once

#include "SystemError.h"

#include <array>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <sys/mman.h>
#include <unistd.h>

namespace Yam {
namespace Http {

namespace Detail {

inline std::size_t MinPagesFor(std::size_t bytes) {
    // see Hacker's Delight 2nd ed. p.59, section 3-1.
    auto pageSize = static_cast<std::size_t>(::getpagesize());
    return (bytes + (-bytes & (pageSize - 1))) / pageSize;
}

} // namespace Detail

template <class T>
class MemoryPool : public std::enable_shared_from_this<MemoryPool<T>> {
public: // public types
    struct Deleter {
        Deleter() = default;

        Deleter(std::shared_ptr<MemoryPool<T>> mp) :
            _mp{std::move(mp)} {}

        void operator()(T* t) {
            _mp->Delete(t);
        }

    private:
        std::shared_ptr<MemoryPool<T>> _mp;
    };

    using Ptr = std::unique_ptr<T, Deleter>;

    template <class U>
    friend class MemoryPool;

protected: // protected types
    union Slot {
        char _data[sizeof(T)];
        Slot* _next;
    };

public: // public functions
    static std::shared_ptr<MemoryPool> Create(std::size_t pages = Detail::MinPagesFor(sizeof(Slot))) {
        return std::shared_ptr<MemoryPool>{new MemoryPool{pages}};
    }

    ~MemoryPool() {
        ::munmap(_buffer, GetBufferSize());
    }

    template <class... Args>
    Ptr New(Args&&... args) {
        auto t = static_cast<T*>(Allocate());

        if (!t)
            throw std::bad_alloc();

        try {
            new(t) T{std::forward<Args>(args)...};
        } catch(...) {
            Deallocate(t);
            throw;
        }

        return {t, this->shared_from_this()};
    }

    void* Allocate() {
        std::lock_guard<decltype(_mutex)> lock{_mutex};

        Slot* slot = _head;

        if (!slot)
            return nullptr; // out of memory

        _head = slot->_next;
        --_freeSlots;

        return slot;
    }

    void Deallocate(void* mem) {
        auto slot = static_cast<Slot*>(mem);

        if (!slot)
            return;

        std::lock_guard<decltype(_mutex)> lock{_mutex};

        slot->_next = _head;
        _head = slot;
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
    static bool IsPageAligned(void* mem) noexcept {
        return 0 == reinterpret_cast<std::uintptr_t>(mem) % ::getpagesize();
    }

    MemoryPool(std::size_t pages) :
        _pages(pages),
        _buffer(CreateBuffer()),
        _head(_buffer),
        _freeSlots(GetCapacity()) {}

    void Delete(T* t) {
        t->~T();
        Deallocate(t);
    }

    Slot* CreateBuffer() {
        if (GetBufferSize() < sizeof(Slot))
            throw std::logic_error("Memory pool buffer size is too small");

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
    Slot* _head;
    std::mutex _mutex;
    std::size_t _freeSlots;
};

// some wizardry to allow MemoryPool<T[N]>,
// including template expectations on its
// returned pointers from New(), etc.

template <class T, std::size_t N>
class MemoryPool<T[N]> : public std::enable_shared_from_this<MemoryPool<T[N]>> {
    using InternalPool = MemoryPool<std::array<T, N>>;

public: // public types
    struct Deleter {
        Deleter() = default;

        Deleter(typename InternalPool::Ptr ptr) :
            _ptr{std::move(ptr)} {}

        void operator()(T* t) {
            _ptr.reset();
        }

    private:
        std::shared_ptr<void> _ptr;
    };

    using Ptr = std::unique_ptr<T[], Deleter>;

public: // public functions
    static std::shared_ptr<MemoryPool> Create(std::size_t pages =
            Detail::MinPagesFor(sizeof(std::array<typename InternalPool::Slot, N>))) {
        return std::shared_ptr<MemoryPool>{new MemoryPool{pages}};
    }

    Ptr New() {
        auto ptr = _mp->New();
        auto data = ptr->data();
        return {data, std::move(ptr)};
    }

    void* Allocate() {
        return _mp->Allocate();
    }

    void Deallocate(void* mem) {
        return _mp->Deallocate(mem);
    }

    std::size_t GetCapacity() const noexcept {
        return _mp->GetCapacity();
    }

    std::size_t GetFreeSlots() const {
        return _mp->GetFreeSlots();
    }

    void* GetBuffer() const noexcept {
        return _mp->GetBuffer();
    }

    std::size_t GetBufferSize() const noexcept {
        return _mp->GetBufferSize();
    }

private:
    MemoryPool(std::size_t pages = 1) :
        _mp{InternalPool::Create(pages)} {}

    std::shared_ptr<InternalPool> _mp;
};

template <class T>
using MemorySlot = typename MemoryPool<T>::Ptr;

} // namespace Http
} // namespace Yam

