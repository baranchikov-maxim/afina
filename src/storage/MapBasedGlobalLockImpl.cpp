#include "MapBasedGlobalLockImpl.h"

#include <mutex>

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Put(const std::string &key, const std::string &value) {
    std::unique_lock<std::mutex> guard(_lock);

    auto iter = _backend.find(key);
    if (iter != _backend.end()) {
        iter->second = value;
        return true;
    } else if (_backend.size() < _max_size) {
        _backend.insert( {key, value} );
        return true;
    } else {
        _backend.erase(_backend.begin());
        _backend.insert( {key, value} );
        return true;
    }
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::PutIfAbsent(const std::string &key, const std::string &value) {
    std::unique_lock<std::mutex> guard(_lock);

    if (_backend.find(key) != _backend.end()) {
        return false;
    }
    return Put(key, value);
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Set(const std::string &key, const std::string &value) {
    std::unique_lock<std::mutex> guard(_lock);

    auto iter = _backend.find(key);
    if (iter == _backend.end()) {
        return false;
    }
    iter->second = value;
    return true;
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Delete(const std::string &key) {
    std::unique_lock<std::mutex> guard(_lock);

    auto iter = _backend.find(key);
    if (iter == _backend.end()) {
        return false;
    }
    _backend.erase(iter);
    return true;
}

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Get(const std::string &key, std::string &value) const {
    std::unique_lock<std::mutex> guard(*const_cast<std::mutex *>(&_lock));

    auto iter = _backend.find(key);
    if (iter == _backend.end()) {
        return false;
    }
    value = iter->second;
    return true;
}


} // namespace Backend
} // namespace Afina
