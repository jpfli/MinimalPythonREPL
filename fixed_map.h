#pragma once

// Simple ordered map - No STL dependencies, compile-time fixed capacity

template<typename Key, typename T, size_t CAPACITY>
class FixedMap {
    
    private:
    
    // Pair type for storing key-value entries
    struct Pair {
        Key key;
        T value;
        
        Pair() : key(), value() {}
        Pair(const Key& k, const T& v) : key(k), value(v) {}
    };
    
    
    public:
    
    using iterator = Pair*;         // Iterator for traversing the map
    using const_iterator = const Pair*;
    using size_type = std::size_t;
    
    // Constructor
    constexpr FixedMap() : _size(0) {}
    
    // --- Core Operations ---
    
    // Find by key - O(log n) using binary search
    // Returns nullptr if not found
    iterator find(const Key& key) {
        size_type pos = lower_bound_index(key);
        if(pos < static_cast<size_type>(_size) && _entries[pos].key == key) {
            return &_entries[pos];
        }
        return end();
    }

    // Const version
    const_iterator find(const Key& key) const {
        size_type pos = lower_bound_index(key);
        if(pos < static_cast<size_type>(_size) && _entries[pos].key == key) {
            return &_entries[pos];
        }
        return cend();
    }
    
    // Subscript operator - creates new entry if key doesn't exist
    // Note: This may construct default T if inserting
    T& operator[](const Key& key) {
        size_type pos = lower_bound_index(key);
        
        if(pos < static_cast<size_type>(_size) && _entries[pos].key == key) {
            return _entries[pos].value;
        }
        
        // Need to insert - check capacity
        if(_size >= CAPACITY) {
            // Full - return reference to last element as fallback
            return _entries[CAPACITY - 1].value;
        }
        
        // Shift elements to make room
        for(size_type i = _size; i > pos; --i) {
            _entries[i] = _entries[i - 1];
        }
        
        _entries[pos] = Pair(key, T());
        ++_size;
        
        return _entries[pos].value;
    }

    // Insert if key doesn't exist - returns true on success
    bool insert(const Key& key, const T& value) {
        size_type pos = lower_bound_index(key);
        
        if(pos < static_cast<size_type>(_size) && _entries[pos].key == key) {
            return false; // Key exists
        }
        
        if(_size >= CAPACITY) {
            return false; // Full
        }
        
        // Shift elements
        for(size_type i = _size; i > pos; --i) {
            _entries[i] = _entries[i - 1];
        }
        
        _entries[pos] = Pair(key, value);
        ++_size;
        
        return true;
    }

    // Erase by key - returns number of elements erased (0 or 1)
    size_type erase(const Key& key) {
        size_type pos = lower_bound_index(key);
        
        if(pos >= static_cast<size_type>(_size) || _entries[pos].key != key) {
            return 0; // Not found
        }
        
        // Shift elements to fill gap
        for(size_type i = pos; i < static_cast<size_type>(_size) - 1; ++i) {
            _entries[i] = _entries[i + 1];
        }
        
        --_size;
        return 1;
    }

    // Check if key exists - O(log n)
    bool contains(const Key& key) const {
        return find(key) != nullptr;
    }

    // Clear all entries
    void clear() {
        _size = 0;
    }

    // Size and capacity
    constexpr size_type size() const { return _size; }
    constexpr size_type capacity() const { return CAPACITY; }
    constexpr bool empty() const { return _size == 0; }
    constexpr bool full() const { return _size >= CAPACITY; }

    // Iterators
    iterator begin() { return iterator(_entries); }
    const_iterator cbegin() const { return iterator(_entries); }
    iterator end() { return iterator(&_entries[_size]); }
    const_iterator cend() const { return iterator(&_entries[_size]); }
    
    
    private:
    
    // Find the first position where key <= entries[pos].key
    // Returns index even if key doesn't exist (insertion point)
    size_type lower_bound_index(const Key& key) const {
        size_type lo = 0;
        size_type hi = _size;
        
        while(lo < hi) {
            size_type mid = lo + (hi - lo) / 2;
            if(_entries[mid].key == key) return mid;
            else if(_entries[mid].key < key) lo = mid + 1;
            else hi = mid;
        }
        
        return lo;
    }

    Pair _entries[CAPACITY];
    size_type _size;
};
