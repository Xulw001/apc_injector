#ifndef SMAP_H
#define SMAP_H

#include <wdm.h>

#include "krtl/unordered_map.h"

template <typename K, typename V>
struct SMap {
    rtl::unordered_map<K, V> map;
    FAST_MUTEX mutex;

    SMap() {
        ExInitializeFastMutex(&mutex);
    }

    ~SMap() {
        ExAcquireFastMutex(&mutex);
        map.clear();
        ExReleaseFastMutex(&mutex);
    }

    bool MapFindValue(const K &key, V &val) {
        bool ret = false;
        ExAcquireFastMutex(&mutex);
        auto it = map.find(key);
        if (it != map.end()) {
            val = it->second;
            ret = true;
        }
        ExReleaseFastMutex(&mutex);
        return ret;
    }

    bool MapAddKey(const K &key, const V &val) {
        ExAcquireFastMutex(&mutex);
        bool ret = map.emplace(key, val).second;
        ExReleaseFastMutex(&mutex);
        return ret;
    }

    bool MapDelKey(const K &key) {
        ExAcquireFastMutex(&mutex);
        bool ret = map.erase(key);
        ExReleaseFastMutex(&mutex);
        return ret;
    }

    template <class... Args>
    bool MapUpdateValue(const K &key,
                        bool (*cb)(typename rtl::unordered_map<K, V>::iterator &, Args...),
                        Args... args) {
        bool ret = false;
        ExAcquireFastMutex(&mutex);
        auto it = map.find(key);
        if (it != map.end()) {
            ret = cb(it, args...);
        }
        ExReleaseFastMutex(&mutex);
        return ret;
    }
};

#endif