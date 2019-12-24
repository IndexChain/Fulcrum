#pragma once

#include "Common.h" // for BadArgs

#include <QCache>
#include <QList>

#include <cassert>
#include <mutex> // for lock_guard
#include <optional>
#include <shared_mutex> // for shared_lock, shared_mutex

/// A cost-based cache, allowing for memory-bounded caching.
///
/// This class is more or less exactly like a QCache, except it is thread-safe. It wraps QCache operations with
/// a std::shared_mutex.  const methods acquire the mutex with a shared lock, whereas non-const methods acquire the
/// mutex exclusively (with the exception of operator[] & object() which take an exclusive lock because they update
/// the LRU linked list).
///
/// Note that we modified the API to QCache.
///
/// 1. We made all the costs be unsigned ints, which is more compatible with C++'s std::size_t and thus produces
///    fewer warnings for the way we will use this cache.
///
/// 2. This class does throw for the following 2 methods: the constructor & setMaxCost, which both require maxCost
///    to be nonzero and smaller than INT_MAX.
///
/// 3. We return the contained object by value rather than as a pointer.  This is because returning a pointer to the
///    object would not be thread safe.  Instead, we copy-construct the returned value with the lock held.  RVO and/or
///    copy elision should make this sufficiently efficient. If client code wants to avoid any copying, it is free to
///    use Qt's implicitly shared containers such as QVector, etc as the value type.
///
/// 4. Inserted objects must be copy-constructible.
///
/// Note: Purging uses an LRU-based strategy, whereby the least recently used ando/or inserted items are purged in a
/// loop until totalCost() < maxCost().  Accessing an item via operator[] or object() will refresh its status as most
/// recently used implicitly (which is why an exclusive lock is used for those two methods).
template <typename Key, typename Value>
class CostCache : protected QCache<Key, Value>
{
    using Base = QCache<Key, Value>;
    using RWLock = std::shared_mutex;
    using ExclusiveLockGuard = std::lock_guard<RWLock>;
    using SharedLockGuard = std::shared_lock<RWLock>;
    mutable RWLock lock;

    void chkMaxCost(unsigned maxCost) noexcept(false) {
        if (!maxCost) throw BadArgs("CostCache cannot use maxCost == 0!");
        if (maxCost >= unsigned(INT_MAX)) throw BadArgs(QString("CostCache cannot have maxCost >= INT_MAX (%1)!").arg(INT_MAX));
    }
public:
    /// May throw if maxCost is 0 or >= INT_MAX
    CostCache(unsigned maxCost) noexcept(false) : Base(maxCost) { chkMaxCost(maxCost); }
    ~CostCache() { clear(); /* paranoia: call our impl. to take the lock to clear */ }

    /// The base size in bytes of a single item in the cache.  Client code can use this base size + whatever extra data
    /// Keys/Values take up to calculate an item's cost in bytes.
    static constexpr size_t itemOverheadBytes() { return sizeof(Key) + sizeof(Value) + sizeof(void *)*4 + sizeof(int); }

    void clear() {
        ExclusiveLockGuard g(lock);
        Base::clear();
    }
    bool contains(const Key & k) const {
        SharedLockGuard g(lock);
        return Base::contains(k);
    }
    unsigned count(const Key & k) const {
        SharedLockGuard g(lock);
        return unsigned(Base::count(k));
    }
    /// Cache takes ownership of `object` and will delete it when this instance is destructed or the cache overflows and
    /// it is purged. Note that this method may implicitly lead to a cache purge if the cache overflows as a result
    /// of this insert. Items whose cost exceeds maxCost will always fail to be inserted.
    /// If this method returns false, `object` is deleted already as a convenience.
    bool insert(const Key & k, Value * object, unsigned cost) {
        assert(cost < unsigned(INT_MAX));
        ExclusiveLockGuard g(lock);
        return Base::insert(k, object, int(cost));
    }
    /// Copy-constructs `v` via new and inserts it into the cache.  A failed insertion will lead to the object being
    /// deleted.
    bool insert(const Key & k, const Value & v, unsigned cost) {
        return insert(k, new Value(v), cost);
    }
    bool isEmpty() const {
        SharedLockGuard g(lock);
        return Base::isEmpty();
    }
    QList<Key> keys() const {
        SharedLockGuard g(lock);
        return Base::keys();
    }
    unsigned maxCost() const {
        SharedLockGuard g(lock);
        return unsigned(Base::maxCost());
    }
    /// Despite this method being const, it takes an exclusive lock because the cache LRU list is modified implicitly.
    /// The returned optional will be empty if the cache lacks item with key `k`, otherwise it will contain a
    /// copy-constructed Value from the cache.
    std::optional<Value> object(const Key & k) const {
        std::optional<Value> ret;
        ExclusiveLockGuard g(lock);
        Value *ptr = Base::object(k);
        if (ptr) ret.emplace(*ptr); // copy-construct the returned value
        return ret;
    }
    bool remove(const Key & k) {
        ExclusiveLockGuard g(lock);
        return Base::remove(k);
    }
    /// May throw if maxCost is 0 or >= INT_MAX
    void setMaxCost(unsigned maxCost) noexcept(false) {
        chkMaxCost(maxCost);
        ExclusiveLockGuard g(lock);
        Base::setMaxCost(int(maxCost));
    }
    unsigned size() const {
        SharedLockGuard g(lock);
        return unsigned(Base::size());
    }
    /// Take an object out of the cache, transfering ownership of it to the caller.  Returns nullptr if `k` was not in
    /// the cache.
    Value *take(const Key & k) {
        ExclusiveLockGuard g(lock);
        return Base::take(k);
    }
    unsigned totalCost() const {
        SharedLockGuard g(lock);
        return unsigned(Base::totalCost());
    }
    /// Despite this method being const, it takes an exclusive lock because the cache LRU list is modified implicitly.
    std::optional<Value> operator[](const Key & k) const { return object(k); }
};
