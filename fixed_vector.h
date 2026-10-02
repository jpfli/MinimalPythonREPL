#pragma once

// Simple vector - No STL dependencies, compile-time fixed capacity

template<typename T, size_t CAPACITY>
class FixedVector {

    public:

    using iterator = T*;
    using const_iterator = const T*;
    using value_type = T;
    using size_type = std::size_t;
    
    // Capacity operations
    constexpr size_type capacity() const noexcept { return CAPACITY; }
    
    constexpr size_type size() const noexcept { return _len; }
    
    constexpr bool empty() const noexcept { return 0 == _len; }
    
    // Element access
    constexpr T& front() { return empty() ? _data[0] : _data[0]; }
    
    constexpr T& back() { return empty() ? _data[0] : _data[_len - 1]; }
    
    constexpr const T& back() const { return empty() ? _data[0] : _data[_len - 1]; }
    
    constexpr T& operator[](size_type index) { return index >= _len ? _data[0] : _data[index]; }
    
    constexpr const T& operator[](size_type index) const { return index >= _len ? _data[0] : _data[index]; }
    
    // Modifiers
    constexpr void push_back(const T& value) {
        if(_len < CAPACITY) {
            _data[_len] = value;
            ++_len;
        }
    }
    
    constexpr void pop_back() { if(!empty()) --_len; }
    
    constexpr iterator insert(iterator pos, size_type count, const T& value) {
        if(pos > end()) pos = end();
        if(count > CAPACITY - (pos - begin())) count = CAPACITY - (pos - begin());
        
        // Shift existing content
        size_type trash = count > CAPACITY - _len ? count - (CAPACITY - _len) : 0;
        for(iterator iter = end() - trash; iter > pos; ) {
            --iter;
            *(iter + count) = *iter;
        }
        _len += count - trash;
        
        // Insert new values
        for(iterator iter = pos; iter < pos + count; ++iter) {
            *iter = value;
        }
        
        return pos;
    }
    
    constexpr iterator erase(iterator first, iterator last) {
        if(first < begin()) first = begin();
        if(last > end()) last = end();
        
        if(first < last) {
            size_type count = last - first;
            for(iterator iter = last; iter < end(); ++iter) {
                *(iter - count) = *iter;
            }
            _len -= count;
        }
        return first;
    }
    
    // Iterators
    constexpr iterator begin() { return _data; }
    
    constexpr const_iterator begin() const { return _data; }
    
    constexpr iterator end() { return _data + _len; }
    
    constexpr const_iterator end() const { return _data + _len; }
    
    // Clear all
    constexpr void clear() { _len = 0; }
    
    // Destructor
    ~FixedVector() = default;
    
    // Copy/move semantics
    FixedVector() = default;
    FixedVector(const FixedVector&) = default;
    FixedVector(FixedVector&&) = default;
    FixedVector& operator=(const FixedVector&) = default;
    FixedVector& operator=(FixedVector&&) = default;
    
    
    private:
    
    T _data[CAPACITY];          // Fixed underlying storage
    size_type _len{0};          // Number of active elements
};
