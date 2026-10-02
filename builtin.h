#pragma once

struct Builtin {
    enum class Kind { INTEGER, FUNCTION, SUBSCRIPT };
    
    Kind kind;
    union {
        int value;                        // INTEGER
        int (*fn)(int arg);               // FUNCTION call operator
        struct {                          // SUBSCRIPT operator
            void* (*fn)(unsigned int i);
            unsigned int width;
        } subscript;
    };
    
    static constexpr Builtin integer(int v) {
        Builtin b{Kind::INTEGER};
        b.value = v;
        return b;
    }
    
    static constexpr Builtin function_op(int (*f)(int arg)) {
        Builtin b{Kind::FUNCTION};
        b.fn = f;
        return b;
    }
    
    static constexpr Builtin subscript_op(void* (*f)(unsigned int i), unsigned int width) {
        Builtin b{Kind::SUBSCRIPT};
        b.subscript.fn = f;
        b.subscript.width = width;  // 1, 2 or 4 bytes
        return b;
    }
};
