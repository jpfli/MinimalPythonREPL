#pragma once

struct Builtin {
    enum class Kind { INTEGER, FUNCTION, SUBSCRIPT };
    
    Kind kind;
    union {
        int32_t value;                  // INTEGER
        int32_t (*fn)(int32_t arg);     // FUNCTION call operator
        struct {                        // SUBSCRIPT operator
            void* (*fn)(uint32_t i);
            uint8_t width;
        } subscript;
    };
    
    static constexpr Builtin integer(int32_t v) {
        Builtin b{Kind::INTEGER};
        b.value = v;
        return b;
    }
    
    static constexpr Builtin function_op(int32_t (*f)(int32_t arg)) {
        Builtin b{Kind::FUNCTION};
        b.fn = f;
        return b;
    }
    
    static constexpr Builtin subscript_op(void* (*f)(uint32_t id), uint8_t width) {
        Builtin b{Kind::SUBSCRIPT};
        b.subscript.fn = f;
        b.subscript.width = width;  // 1, 2 or 4 bytes
        return b;
    }
};
