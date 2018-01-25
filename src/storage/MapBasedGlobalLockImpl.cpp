#include "MapBasedGlobalLockImpl.h"

#include <mutex>
#include <algorithm>

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool MapBasedGlobalLockImpl::Put(const std::string &key, const std::string &value) {
    std::unique_lock<std::mutex> guard(_lock);

    auto iter = _backend.find(key);
    if (iter != _backend.end()) {
        iter->second = value;
        auto hist_iter = std::find(_history.begin(), _history.end(), key);
        _history.erase(hist_iter);
        _history.push_back(key);
        return true;

    } else if (_backend.size() < _max_size) {
        _backend.insert( {key, value} );
        _history.push_back(key);
        return true;

    } else {
        _backend.erase( _history.front() );
        _history.pop_front();

        _backend.insert( {key, value} );
        _history.push_back(key);
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

    auto hist_it = std::find(_history.begin(), _history.end(), key);
    _history.erase(hist_it);
    _history.push_back(key);

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

    auto hist_it = std::find(_history.begin(), _history.end(), key);
    _history.erase(hist_it);
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
