#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

void SimpleLRU::free_head() {
    _space_left += (_lru_head->key.length() + _lru_head->value.length());
    lru_node* next_head = _lru_head->next.get();
    _lru_head->next.release();
    _lru_index.erase(_lru_head->key);
    _lru_head.reset(next_head);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string& key, const std::string &value) {
    if(key.length() + value.length() > _max_size) {
       return false;
    }
    auto it = _lru_index.find(key);
    if (it != _lru_index.end()) {
        lru_node& our_node = it->second.get();
        while((value.length() - our_node.value.length() > _space_left) && (_lru_head->key != key)) {
            free_head();
        }
        if (_lru_head->key == key) {
            free_head();
            it = _lru_index.end();
        }
        else {
            _space_left -= (value.length() - our_node.value.length());
            our_node.value = value;
        }
    }
    if (it == _lru_index.end()) {
        while(value.length() + key.length() > _space_left) {
             free_head();
        }
        if (_lru_head) {
            _lru_tail->next.reset(new lru_node(key,value));
            _lru_tail->next->prev = _lru_tail;
            _lru_tail = _lru_tail->next.get();
            _lru_index.emplace(_lru_tail->key, *_lru_tail);
         }
         else {
            _lru_head.reset(new lru_node(key,value));
            _lru_tail = _lru_head.get();
            _lru_index.emplace(_lru_tail->key, *_lru_tail);
        }
        _space_left -= (key.length() + value.length());
    }
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    if(key.length() + value.length() > _max_size) {
       return false;
    }
    auto it = _lru_index.find(key);
    if (it != _lru_index.end()) {
        return false;
    }
    if (it == _lru_index.end()) {
        while(value.length() + key.length() > _space_left) {
             free_head();
        }
        if (_lru_head) {
            _lru_tail->next.reset(new lru_node(key,value));
            _lru_tail->next->prev = _lru_tail;
            _lru_tail = _lru_tail->next.get();
            _lru_index.emplace(_lru_tail->key, *_lru_tail);
         }
         else {
            _lru_head.reset(new lru_node(key,value));
            _lru_tail = _lru_head.get();
            _lru_index.emplace(_lru_tail->key, *_lru_tail);
        }
        _space_left -= (key.length() + value.length());
    }
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    if(key.length() + value.length() > _max_size) {
       return false;
    }
    auto it = _lru_index.find(key);
    if (it == _lru_index.end()) {
        return false;
    }
    if (it != _lru_index.end()) {
        lru_node& our_node = it->second.get();
        while((value.length() - our_node.value.length() > _space_left) && (_lru_head->key != key)) {
            free_head();
        }
        _space_left -= (value.length() - our_node.value.length());
        our_node.value = value;
    }
    return true;
}



// See MapBasedGlobalLockImpl.true
bool SimpleLRU::Delete(const std::string &key) {
    auto it = _lru_index.find(key);
    if(it == _lru_index.end()) {
            return false;
    }
    lru_node& our_node = it->second.get();
    _space_left += key.length() + our_node.value.length();
    lru_node* next_node = our_node.next.get();
    _lru_index.erase(key);
    if(next_node) {
        next_node->prev = our_node.prev;
    }
    else {
        _lru_tail = our_node.prev;
    }
    if(_lru_head->key == key) {
        _lru_head->next.release();
        _lru_head.reset(next_node);
    }
    else {
        our_node.prev->next.reset(next_node);
    }
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) {
    auto it = _lru_index.find(key);
    if(it == _lru_index.end()) {
            return false;
     }
     lru_node& our_node = it->second.get();
     value = our_node.value;
     return true;
  }

}
}
