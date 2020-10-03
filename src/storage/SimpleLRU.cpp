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

void SimpleLRU::to_tail(lru_node& node) {
    if (node.key != _lru_tail->key) {
        lru_node* next_node = node.next.get();
        next_node->prev = node.prev;
        if(_lru_head->key == node.key) {
            _lru_tail->next.release();
            _lru_tail->next.reset(_lru_head.get());
            _lru_head.release();
            _lru_head.reset(next_node);
        }
        else {
            _lru_tail->next.release();
            _lru_tail->next.reset(node.prev->next.get());
            node.prev->next.release();
            node.prev->next.reset(next_node);
        }
        _lru_tail->next->prev = _lru_tail;
        _lru_tail = _lru_tail->next.get();
        node.next.release();
    }
}

void SimpleLRU::set_node(lru_node& node, const std::string &value) {
    to_tail(node);
    while((value.length() > _space_left + node.value.length())) {
        free_head();
    }
    _space_left -= (value.length() - node.value.length());
    node.value = value;
}

void SimpleLRU::add_node(const std::string &key, const std::string &value) {
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

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string& key, const std::string &value) {
    if(key.length() + value.length() > _max_size) {
       return false;
    }
    auto it = _lru_index.find(key);
    if (it != _lru_index.end()) {
        lru_node& our_node = it->second.get();
        set_node(our_node, value);
    }
    else {
        add_node(key, value);
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
        add_node(key, value);
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
        set_node(our_node, value);
    }
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
    auto it = _lru_index.find(key);
    if(it == _lru_index.end()) {
            return false;
    }
    lru_node& our_node = it->second.get();
    _space_left += key.length() + our_node.value.length();
    lru_node* next_node = our_node.next.get();
    _lru_index.erase(it);
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
     to_tail(our_node);
     return true;
  }

} // namespace Backend
} // namespace Afina
