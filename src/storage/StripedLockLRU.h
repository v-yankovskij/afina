#ifndef AFINA_STORAGE_STRIPED_LOCK_LRU_H
#define AFINA_STORAGE_STRIPED_LOCK_LRU_H

#include <map>
#include <mutex>
#include <string>
#include <functional>
#include <vector>

#include "ThreadSafeSimpleLRU.h"

namespace Afina {
namespace Backend {

/**
 * # LRU thread safe version with striped locks
 *
 *
 */
class StripedLockLRU: public Afina::Storage {
public:
    StripedLockLRU(size_t shard_size = 1<<20, size_t num_shards = 4): _shard_size(shard_size), _num_shards(num_shards) {
         for(size_t i = 0; i < _num_shards; i++) {
             _shards.emplace_back(new ThreadSafeSimplLRU(_shard_size));
         }
    }

    ~StripedLockLRU() {}

    // see SimpleLRU.h
    bool Put(const std::string &key, const std::string &value) override {
        return _shards[_hash(key)%_num_shards]->Put(key, value);
    }

    // see SimpleLRU.h
    bool PutIfAbsent(const std::string &key, const std::string &value) override {
        return _shards[_hash(key)%_num_shards]->PutIfAbsent(key, value);
    }

    // see SimpleLRU.h
    bool Set(const std::string &key, const std::string &value) override {
        return _shards[_hash(key)%_num_shards]->Set(key, value);
    }

    // see SimpleLRU.h
    bool Delete(const std::string &key) override  {
        return _shards[_hash(key)%_num_shards]->Delete(key);
    }

    // see SimpleLRU.h
    bool Get(const std::string &key, std::string &value) override  {
        return _shards[_hash(key)%_num_shards]->Get(key, value);
    }

private:
    std::hash<std::string> _hash;
    size_t _shard_size;
    size_t _num_shards;
    std::vector<std::unique_ptr<ThreadSafeSimplLRU>> _shards;
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_STRIPED_LOCK_LRU_H
