#include <gmock/gmock.h>

#include "MemoryPool.h"

#include <random>
#include <thread>
#include <vector>

using namespace ::testing;

namespace Yam {
namespace Http {

struct Type {
    Type() {}
    Type(char w, int x, int y, int z) :
        w(w), x(x), y(y), z(z) {}

    char w;
    int x;
    int y;
    int z;
};

class DestructionVerifier {
public:
    enum class Handle {
        Live,
        Destroyed
    };

    DestructionVerifier(Handle& h) :
        _h(&h) {
        *_h = Handle::Live;
    }

    ~DestructionVerifier() {
        *_h = Handle::Destroyed;
    }

private:
    Handle* _h;
};

class MemoryPoolTest : public Test {
public:
    MemoryPoolTest() {
    }

protected:
    void AllocDeallocRandomly(MemoryPool<Type>& mp) {
        std::vector<void*> ptrs;
        std::random_device rd;

        auto dealloc = [&] {
            auto index = rd() % ptrs.size();
            auto ptr = ptrs.at(index);
            ptrs.erase(begin(ptrs) + index);
            mp.Deallocate(ptr);
        };

        std::vector<std::function<void()>> actions = {
            [&] {
                if (mp.GetFreeSlots())
                    ptrs.push_back(mp.Allocate());
                else if (ptrs.size())
                    dealloc();
            },

            [&] {
                if (ptrs.size())
                    dealloc();
                else
                    ptrs.push_back(mp.Allocate());
            }
        };

        for (int i = 0; i < 100000; i++)
            actions[rd() % 2]();

        while (ptrs.size())
            dealloc();
    }
};

TEST_F(MemoryPoolTest, alloc_dealloc) {
    auto mp = MemoryPool<Type>::Create();

    void* mem = mp->Allocate();
    mp->Deallocate(mem);
}

TEST_F(MemoryPoolTest, use_memory) {
    auto mp = MemoryPool<Type>::Create();

    Type* t = static_cast<Type*>(mp->Allocate());
    t->w = 'a';
    t->x = 1;
    t->y = 2;
    t->z = 3;
    mp->Deallocate(t);
}

TEST_F(MemoryPoolTest, alloc_and_construct) {
    auto mp = MemoryPool<Type>::Create();

    auto t = mp->New('a', 1, 2, 3);

    EXPECT_EQ('a', t->w);
    EXPECT_EQ(1, t->x);
    EXPECT_EQ(2, t->y);
    EXPECT_EQ(3, t->z);
}

TEST_F(MemoryPoolTest, new_returns_smart_ptr) {
    DestructionVerifier::Handle handle;

    auto mp = MemoryPool<DestructionVerifier>::Create();
    mp->New(handle);
    EXPECT_EQ(DestructionVerifier::Handle::Destroyed, handle);
}

TEST_F(MemoryPoolTest, shared_ptr_from_smart_ptr) {
    DestructionVerifier::Handle handle;

    auto mp = MemoryPool<DestructionVerifier>::Create();
    std::shared_ptr<DestructionVerifier>{mp->New(handle)};
    EXPECT_EQ(DestructionVerifier::Handle::Destroyed, handle);
}

TEST_F(MemoryPoolTest, alloc_dealloc_all) {
    auto mp = MemoryPool<Type>::Create();

    std::vector<void*> ptrs;

    while (mp->GetFreeSlots())
        ptrs.push_back(mp->Allocate());

    for (auto ptr : ptrs)
        mp->Deallocate(ptr);

    EXPECT_EQ(mp->GetCapacity(), mp->GetFreeSlots());
}

TEST_F(MemoryPoolTest, alloc_dealloc_randomly) {
    auto mp = MemoryPool<Type>::Create();
    AllocDeallocRandomly(*mp);
    EXPECT_EQ(mp->GetCapacity(), mp->GetFreeSlots());
}

TEST_F(MemoryPoolTest, concurrent_alloc_dealloc_randomly) {
    auto mp = MemoryPool<Type>::Create();

    std::vector<std::unique_ptr<std::thread>> threads;

    for (int i = 0; i < 5; i++)
        threads.emplace_back(new std::thread{[&] {
                AllocDeallocRandomly(*mp);
        }});

    for (auto& t : threads)
        t->join();

    EXPECT_EQ(mp->GetCapacity(), mp->GetFreeSlots());
}

TEST_F(MemoryPoolTest, live_slots_outlive_pool) {
    MemorySlot<DestructionVerifier> slot;
    DestructionVerifier::Handle handle;

    {
        slot = MemoryPool<DestructionVerifier>::Create()->New(handle);
    }

    EXPECT_EQ(DestructionVerifier::Handle::Live, handle);

    slot.reset();

    EXPECT_EQ(DestructionVerifier::Handle::Destroyed, handle);
}

} // namespace Http
} // namespace Yam

