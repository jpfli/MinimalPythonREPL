#pragma once

#include <cstddef>
#include <cstdint>

// Lightweight non-owning string view -
// No STL dependencies, similar interface to std::string_view

class StringView {

public:

    using char_type = char;
    using size_type = std::size_t;
    using const_iterator = const char*;

    // Constants
    static constexpr size_type npos = static_cast<size_type>(-1);

    // --- Constructors ---

    // Default constructor - empty view
    constexpr StringView() : _data(nullptr), _len(0) {}

    // Constructor from C-string (null-terminated)
    constexpr explicit StringView(const char_type* str) : _data(str), _len(str ? _compute_length(str) : 0) {}

    // Constructor from buffer + explicit length
    constexpr StringView(const char_type* str, size_type length)
        : _data(str), _len(length) {}

    // Constructor from pointer range (begin inclusive, end exclusive)
    constexpr StringView(const char_type* begin, const char_type* end) : _data(begin), _len(static_cast<size_type>(end - begin)) {}

    // Copy constructor
    constexpr StringView(const StringView& other) = default;

    // Move constructor
    constexpr StringView(StringView&& other) = default;

    // Assignment operators
    constexpr StringView& operator=(const StringView& other) = default;
    constexpr StringView& operator=(StringView&& other) = default;

    // Assignment from C-string
    constexpr StringView& operator=(const char_type* str) {
        _data = str;
        _len = str ? _compute_length(str) : 0;
        return *this;
    }

    // --- Element Access ---

    // Get character at position
    constexpr const char_type& operator[](size_type pos) const {
        return _data[pos];
    }

    // Get character at position with bounds checking
    constexpr const char_type& at(size_type pos) const {
        return pos < _len ? _data[pos] : _data[0];  // Return first on out-of-bounds
    }

    // Get raw pointer to underlying buffer
    constexpr const char_type* data() const {
        return _data;
    }

    // --- Size Information ---

    constexpr size_type size() const { return _len; }
    constexpr size_type length() const { return _len; }
    constexpr bool empty() const { return _len == 0; }

    // --- Iterators ---

    constexpr const_iterator begin() const { return _data; }
    constexpr const_iterator end() const { return _data + _len; }
    constexpr const_iterator cbegin() const { return _data; }
    constexpr const_iterator cend() const { return _data + _len; }

    // --- Substring Operations ---

    // Extract a substring view
    constexpr StringView substr(size_type pos=0, size_type count=npos) const {
        if(pos >= _len) return StringView();
        if(count > _len - pos) count = _len - pos;
        return StringView(_data + pos, count);
    }
    
    constexpr StringView remove_prefix(size_type count) const {
        if(count >= _len) return StringView();
        return StringView(_data + count, _len - count);
    }
    
    constexpr StringView remove_suffix(size_type count) const {
        if(count >= _len) return StringView();
        return StringView(_data, _len - count);
    }
    
    // --- Mutating versions (modify this view) ---
    
    void remove_prefix_mut(size_type count) {
        if(count >= _len) {
            _data = nullptr;
            _len = 0;
        } else {
            _data += count;
            _len -= count;
        }
    }

    void remove_suffix_mut(size_type count) {
        if(count >= _len) {
            _data = nullptr;
            _len = 0;
        } else {
            _len -= count;
        }
    }

    // Lexicographical comparison
    constexpr int compare(const StringView& other) const {
        size_type min_len = _len < other._len ? _len : other._len;
        for(size_type i = 0; i < min_len; ++i) {
            int c = (_data[i] > other._data[i]) - (_data[i] < other._data[i]);
            if(0 != c) return c;
        }
        return (_len > other._len) - (_len < other._len);
    }

    // --- Searching ---

    // Find first occurrence of character
    constexpr size_type find(char_type ch, size_type pos=0) const {
        for(size_type i = pos; i < _len; ++i) {
            if(_data[i] == ch) return i;
        }
        return npos;
    }

    // Find first occurrence of another view/string
    constexpr size_type find(const StringView& str, size_type pos=0) const {
        for(size_type i = pos; i <= _len - str._len; ++i) {
            bool match = true;
            for(size_type j = 0; j < str._len; ++j) {
                if(_data[i + j] != str._data[j]) {
                    match = false;
                    break;
                }
            }
            if(match) return i;
        }
        return npos;
    }

    // Find last occurrence
    constexpr size_type rfind(char_type ch, size_type pos=npos) const {
        for(size_type i = (pos >= _len) ? _len - 1 : pos; i != npos; --i) {
            if(_data[i] == ch) return i;
        }
        return npos;
    }

    size_type rfind(const StringView& str, size_type pos=npos) const {
        if(_len < str._len) return npos;
        if(pos > _len - str._len) pos = _len - str._len;
        
        while(pos != npos) {
            bool match = true;
            for(size_type j = 0; j < str._len; ++j) {
                if(_data[pos + j] != str[j]) {
                    match = false;
                    break;
                }
            }
            if(match) return pos;
            --pos;
        }
        return npos;
    }


private:

    // Compute length of null-terminated string
    static constexpr size_type _compute_length(const char_type* str) {
        size_type len = 0;
        while(str[len] != '\0') ++len;
        return len;
    }

    // Internal storage
    const char_type* _data;
    size_type _len;
};

// --- Comparison operators ---

constexpr bool operator==(const StringView& lhs, const StringView& rhs) {
    return lhs.compare(rhs) == 0;
}

constexpr bool operator!=(const StringView& lhs, const StringView& rhs) {
    return lhs.compare(rhs) != 0;
}

constexpr bool operator<(const StringView& lhs, const StringView& rhs) {
    return lhs.compare(rhs) < 0;
}

constexpr bool operator<=(const StringView& lhs, const StringView& rhs) {
    return lhs.compare(rhs) <= 0;
}

inline constexpr bool operator>(const StringView& lhs, const StringView& rhs) {
    return lhs.compare(rhs) > 0;
}

inline constexpr bool operator>=(const StringView& lhs, const StringView& rhs) {
    return lhs.compare(rhs) >= 0;
}

