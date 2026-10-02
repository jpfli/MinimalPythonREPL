#pragma once

#include <cstdio>
#include <cstdlib>

#include "Lexer.h"
#include "fixed_vector.h"
#include "string_view.h"
#include "fixed_map.h"
#include "builtin.h"

// Expression evaluator using Pratt parsing (top-down operator precedence parsing)
// 
// Provides a lightweight expression parser and evaluator. It supports:
//   - Integer literals (decimal, hex with 0x, octal with 0o, binary with 0b)
//   - Single-character user variables
//   - Built-in functions and subscriptables (memory access)
//   - Unary operators (~, -, +)
//   - Binary operators (+, -, *, /, &, ^, |)
//   - Assignment (=)
//   - Subscripting ([index]) for memory access
//   - Function calls ((argument))
// 
//  Memory Management: Uses a FixedVector arena allocator for parsed expressions,
//  which means all parsed expressions are valid only until the next parse() call
//  clears the arena.

class Evaluator {
    
    public:
    
    /**
     * Represents a parsed expression node in the abstract syntax tree
     * 
     * Uses a discriminated union to store different expression types efficiently.
     * All expression nodes are owned by the Evaluator's arena allocator.
     */
    struct Expression {
        enum class Type : int {
            EMPTY, INTEGER, IDENTIFIER, ASSIGN, UNARY_OP, BINARY_OP, INDEX_OP
        };
        
        // The type of this expression node
        Type type;
        
        // Union holding type-specific data - Only the member matching 'type' is valid at any time
        union {
            long unsigned int value;        // IINTEGER: Literal value
            StringView identifier;          // IDENTIFIER: Variable/builtin name
            struct {                        // UNARY_OP/BINARY_OP/ASSIGN: Operator info
                char op;                            // Operator character ('+', '-', '[', '(', etc.)
                union {
                    const Expression* operand;      // UNARY_OP: Single operand
                    struct {                        // BINARY_OP/ASSIGN: Two operands
                        const Expression* left;     // Left operand
                        const Expression* right;    // Right operand
                    };
                };
            };
        };
        
        // Default constructor: creates an EMPTY expression
        Expression() : type(Type::EMPTY) {};
        
        // Construct integer literal
        Expression(Type t, long unsigned int v) : type(t), value(v) {}
        
        // Construct identifier reference
        Expression(Type t, const StringView& sv) : type(t), identifier(sv) {}
        
        // Construct unary operator expression
        Expression(Type t, char o, const Expression* p) : type(t), op(o), operand(p) {}
        
        // Construct binary operator or assignment expression
        Expression(Type t, char o, const Expression* l, const Expression* r) : type(t), op(o), left(l), right(r) {}
        
        // Copy assignment operator - Copies the expression based on its type
        Expression& operator=(const Expression& other) {
            type = other.type;
            switch(type) {
                case Type::INTEGER: value = other.value; break;
                case Type::IDENTIFIER: identifier = other.identifier; break;
                case Type::UNARY_OP:
                    op = other.op;
                    operand = other.operand;
                    break;
                case Type::BINARY_OP: case Type::ASSIGN:
                    op = other.op;
                    left = other.left;
                    right = other.right;
                    break;
            }
            return *this;
        }
    };
    
    // Default constructor: initializes empty arena allocator
    explicit Evaluator() : _arena() {}
    
    // Default destructor
    ~Evaluator() = default;
    
    /**
     * Parse a source string into an expression tree using Pratt parsing
     *   (reference: https://inv.nadeko.net/watch?v=0c8b7YfsBKs)
     * 
     * 'str'   Source code to parse
     * 'error' Output parameter for error messages (nullptr if successful)
     * 
     * Returns pointer to root Expression node, or nullptr on parse failure.
     * The returned Expression pointers are only valid until the next parse() call
     */
    const Expression* parse(const char* str, const char* (&error)) {
        // Clear the expression arena for fresh parsing
        _arena.clear();
        
        // Initialize lexer with source string
        Lexer lex(str);
        
        // Handle empty input (just end-of-line)
        if(lex.peek() == Lexer::Token::END_OF_LINE) {
            _arena.push_back(Expression());
            return &_arena.back();
        }
        
        // Parse the full expression
        const auto expr = _parse_expression(lex, error);
        
        // Check for unmatched parentheses after parsing
        auto token = lex.peek();
        if(Lexer::Token::RPAREN == token) {
            error = "unmatched ')'";
            return nullptr;
        }
        if(Lexer::Token::RBRACKET == token) {
            error = "unmatched ']'";
            return nullptr;
        }
        
        return expr;
    }
    
    /**
     * Evaluate an expression tree to produce a 'long integer' result
     * 
     * Recursively evaluates the abstract syntax tree (AST), resolving variables,
     * calling builtins, and performing arithmetic/bitwise operations.
     * 
     * 'expr'     Root of the expression tree to evaluate
     * 'error'    Output parameter for error messages (nullptr if successful)
     * 'vars'     Map of single-char user variables to their integer values
     * 'builtins' Map of builtin identifiers to their function implementations
     * 
     * Returns Evaluation result, or 0 on error
     */
    long int evaluate(const Expression* expr, const char* (&error), FixedMap<char, long int, 53>& vars, const FixedMap<StringView, Builtin, 5>& builtins) {
        switch(expr->type) {
            case Expression::Type::EMPTY:
                // Empty expression evaluates to 0
                return 0;
            
            case Expression::Type::INTEGER:
                // Integer literals return their stored value
                return expr->value;
            
            case Expression::Type::IDENTIFIER: {
                // Try to resolve as a single-character user variable first
                if(1 == expr->identifier.size()) {
                    auto var_iter = vars.find(expr->identifier[0]);
                    if(vars.cend() != var_iter) return var_iter->value;
                }
                
                // Then try to resolve as a builtin identifier
                auto builtin_iter = builtins.find(expr->identifier);
                if(builtins.cend() != builtin_iter) {
                    if(Builtin::Kind::INTEGER == builtin_iter->value.kind) return builtin_iter->value.value;
                    else if(Builtin::Kind::FUNCTION == builtin_iter->value.kind) error = "expected '(' after callable";
                    else if(Builtin::Kind::SUBSCRIPT == builtin_iter->value.kind) error = "expected '[' after subscriptable";
                    else error = "undefined variable";
                }
                else {
                    // Not found in either map
                    error = "undefined variable";
                }
                
                return 0;
            }
            case Expression::Type::ASSIGN: {
                // Assignment: evaluate RHS first, then assign to LHS
                const auto rval = evaluate(expr->right, error, vars, builtins);
                if(error) return 0;
                
                auto lhs = expr->left;
                if(Expression::Type::IDENTIFIER == lhs->type) {
                    // Simple variable assignment: x = 4
                    if(1 < lhs->identifier.size()) {
                        error = "user variable names must be single-character";
                        return 0;
                    }
                    vars[lhs->identifier[0]] = rval;
                }
                else if(Evaluator::Expression::Type::BINARY_OP == lhs->type && '[' == lhs->op) {
                    // Memory store via subscript: mem[index] = 4
                    const auto builtin_iter = builtins.find(lhs->left->identifier);
                    if(builtins.cend() == builtin_iter) {
                        error = "undefined identifier";
                        return 0;
                    }
                    else if(Builtin::Kind::SUBSCRIPT != builtin_iter->value.kind) {
                        error = "not subscriptable";
                        return 0;
                    }
                    
                    // Evaluate the index expression
                    const auto index = evaluate(lhs->right, error, vars, builtins);
                    if(error) return 0;
                    
                    // Get pointer to memory location
                    void* ptr = builtin_iter->value.subscript.fn(index);
                    
                    // Store value with appropriate bit-width based on builtin configuration
                    switch(builtin_iter->value.subscript.width) {
                        case 1: *static_cast<uint8_t*>(ptr) = rval; break;
                        case 2: *static_cast<uint16_t*>(ptr) = rval; break;
                        default: *static_cast<int32_t*>(ptr) = rval;
                    }
                }
                else {
                    // Invalid assignment target
                    error = "invalid assign";
                    return 0;
                }
                return rval;
            }
            case Expression::Type::UNARY_OP: {
                // Evaluate operand first, then apply operator
                const auto val = evaluate(expr->operand, error, vars, builtins);
                if(error) return 0;
                
                switch(expr->op) {
                    case '~': return ~val;      // Bitwise NOT
                    case '-': return -val;      // Negation
                    case '+': return val;       // Unary plus (identity)
                    default: return 0;          // Unknown operator
                }
            }
            case Expression::Type::BINARY_OP: {
                // Handling for subscript operator [index]
                if('[' == expr->op) {
                    auto builtin_iter = builtins.find(expr->left->identifier);
                    if(builtins.cend() == builtin_iter) {
                        error = "undefined identifier";
                        return 0;
                    }
                    if(Builtin::Kind::SUBSCRIPT != builtin_iter->value.kind) {
                        error = "not subscriptable";
                        return 0;
                    }
                    
                    // Evaluate the index
                    const auto index = evaluate(expr->right, error, vars, builtins);
                    if(error) return 0;
                    
                    // Load value from memory with appropriate bit-width
                    void* ptr = builtin_iter->value.subscript.fn(index);
                    switch(builtin_iter->value.subscript.width) {
                        case 1: return *static_cast<uint8_t*>(ptr);
                        case 2: return *static_cast<uint16_t*>(ptr);
                        default: return *static_cast<int32_t*>(ptr);
                    }
                }
                else if('(' == expr->op) {
                    // Special handling for function call operator (arg)
                    auto builtin_iter = builtins.find(expr->left->identifier);
                    if(builtins.cend() != builtin_iter) {
                        error = "undefined identifier";
                        return 0;
                    }
                    if(Builtin::Kind::FUNCTION != builtin_iter->value.kind) {
                        error = "not callable";
                        return 0;
                    }
                    
                    // Evaluate argument and call the builtin function
                    const auto arg = evaluate(expr->right, error, vars, builtins);
                    if(error) return 0;
                    
                    return builtin_iter->value.fn(arg);
                }
                else {  // Standard binary operators
                    // Evaluate both operands
                    const auto lval = evaluate(expr->left, error, vars, builtins);
                    if(error) return 0;
                    
                    const auto rval = evaluate(expr->right, error, vars, builtins);
                    if(error) return 0;
                    
                    // Apply the appropriate operator
                    switch(expr->op) {
                        case '|': return lval | rval;       // Bitwise OR
                        case '^': return (lval ^ rval);     // Bitwise XOR
                        case '&': return lval & rval;       // Bitwise AND
                        case '+': return lval + rval;       // Addition
                        case '-': return lval - rval;       // Subtraction
                        case '*': return lval * rval;       // Multiplication
                        case '/': return lval / rval;       // Division
                        default:
                            error = "invalid operator";
                            return 0;
                    }
                }
            }
            default:
                // Unknown expression type
                error = "evaluation error";
                return 0;
        }
    }
    
    
    private:
    
    /**
     * Recursive descent parser implementing Pratt parsing algorithm
     * 
     * This method parses expressions with correct operator precedence and associativity.
     * The 'min_bind' parameter implements Pratt parsing by controlling when to stop
     * consuming infix operators - operators with binding power <= min_bind are not consumed.
     * 
     * Operator Precedence (binding power):
     *   '|' (OR):     10 - lowest precedence
     *   '^' (XOR):    20
     *   '&' (AND):    30
     *   '+' '-' :     40
     *   '*' '/' :     50 - highest precedence
     * 
     * 'lex'      Lexer instance providing tokens from source
     * 'error'    Output parameter for error messages
     * 'min_bind' Minimum binding power threshold (default 0 = accept all operators)
     *
     * Returns pointer to parsed expression, or nullptr on error
     */
    const Expression* _parse_expression(Lexer& lex, const char* (&error), int min_bind=0) {
        const Expression* left = nullptr;
        auto token = lex.peek();
        if(Lexer::Token::NUMBER == token) {
            // Parse integer literal
            _arena.push_back(Expression(Expression::Type::INTEGER, _parse_int(lex.token_pos())));
            lex.consume();
            left = &_arena.back();
        }
        else if(Lexer::Token::IDENTIFIER == token) {
            // Parse identifier (user defined variable or builtin reference)
            StringView token_str(lex.token_pos(), lex.token_len());
            _arena.push_back(Expression(Expression::Type::IDENTIFIER, token_str));
            lex.consume();
            left = &_arena.back();
        }
        else if((Lexer::Token::MINUS == token) || (Lexer::Token::PLUS == token) || (Lexer::Token::NOT == token)) {
            // Parse unary operator (~, -, +)
            char op = *lex.token_pos();
            lex.consume();
            
            // Unary operators have high precedence (40)
            const auto operand = _parse_expression(lex, error, 40);
            if(error) return nullptr;
            
            _arena.push_back(Expression(Expression::Type::UNARY_OP, op, operand));
            left = &_arena.back();
        }
        else if(Lexer::Token::LPAREN == token) {
            // Parse parenthesized expression: (expr)
            lex.consume();
            left = _parse_expression(lex, error, 0);
            
            // Expect closing ')' after expression
            if(!lex.consume_if(Lexer::Token::RPAREN)) {
                error = "expected ')'";
                return nullptr;
            }
        }
        else if(Lexer::Token::END_OF_LINE == token) {
            // Unexpected end of line in middle of expression
            error = "end of line";
            return nullptr;
        }
        else {
            // Unknown token
            error = "invalid syntax";
            return nullptr;
        }
        
        // Continue consuming operators while they have higher precedence than 'min_bind'
        while(true) {
            token = lex.peek();
            
            // Termination conditions: end of input or closing delimiters
            if(Lexer::Token::END_OF_LINE == token || Lexer::Token::RPAREN == token || Lexer::Token::RBRACKET == token) {
                break;
            }
            // Assignment operator: lower precedence, binds everything to its left
            else if(Lexer::Token::ASSIGN == token) {
                lex.consume();      // Consume '=' token
                const auto right = _parse_expression(lex, error);
                if(error) return nullptr;
                
                _arena.push_back(Expression(Expression::Type::ASSIGN, '=', left, right));
            }
            // Subscript operator: [index]
            else if(Lexer::Token::LBRACKET == token) {
                lex.consume();      // Consume '[' token
                const auto right = _parse_expression(lex, error);
                if(error) return nullptr;
                
                // Expect closing ']' after index expression
                if(!lex.consume_if(Lexer::Token::RBRACKET)) {
                    error = "expected ']'";
                    return nullptr;
                }
                _arena.push_back(Expression(Expression::Type::BINARY_OP, '[', left, right));
            }
            // Function call operator: (argument)
            else if(Lexer::Token::LPAREN == token) {
                lex.consume();      // Consume '(' token
                const auto right = _parse_expression(lex, error);
                if(error) return nullptr;
                
                // Expect closing ')' after argument expression
                if(!lex.consume_if(Lexer::Token::RPAREN)) {
                    error = "expected ')'";
                    return nullptr;
                }
                _arena.push_back(Expression(Expression::Type::BINARY_OP, '(', left, right));
            }
            // Standard binary operators: +, -, *, /, &, ^, |
            else if((Lexer::Token::PLUS == token) || (Lexer::Token::MINUS == token) || 
                    (Lexer::Token::MUL == token) || (Lexer::Token::DIV == token) || 
                    (Lexer::Token::OR == token) || (Lexer::Token::XOR == token) || 
                    (Lexer::Token::AND == token)) {
                char op = *lex.token_pos();
                int bind = _infix_binding(op);
                
                // If this operator has lower/equal precedence than current level, stop
                if(bind <= min_bind) break;
                
                lex.consume();      // Consume operator token
                
                // Recurse with this operator's precedence as new minimum
                const auto right = _parse_expression(lex, error, bind);
                if(!right) {
                    error = "invalid syntax";
                    return nullptr;
                }
                
                _arena.push_back(Expression(Expression::Type::BINARY_OP, op, left, right));
            }
            else {
                // Unexpected token
                error = "invalid syntax";
                return nullptr;
            }
            
            // Update 'left' to point to newly created node for next iteration
            left = &_arena.back();
        }
        return left;
    }
    
    /**
     * Returns the binding power (precedence) of a binary operator
     * 
     * Higher values indicate tighter binding (higher precedence).
     * Used by Pratt parsing to determine operator order of operations.
     * 
     * 'op' The operator character
     * 
     * Returns binding power value (10-50)
     */
    constexpr static int _infix_binding(char op) {
        switch(op) {
            case '|': return 10;
            case '^': return 20;
            case '&': return 30;
            case '+': case '-': return 40;
            case '*': case '/': return 50;
        }
        return 0;   // Unknown operator
    }
    
    /**
     * Parse an integer literal from source string
     * 
     * Handles multiple numeric bases:
     *   - Decimal, hexadecimal (0x), octal (0o), binary (0b)
     * Literal may contain '_' separators between digits
     * 
     * 'str' Pointer to start of number in source
     * 
     * Returns parsed integer value (0 if invalid)
     */
    constexpr static long unsigned int _parse_int(const char* str) {
        // Validate first digit
        if('0' > *str || '9' < *str) return 0;
        
        int digit = *str;
        long unsigned int val = digit - '0';
        ++str;
        
        // Determine numeric base from prefix
        int base = 10;
        if('0' == digit) {
            switch(*str) {
                case 'x': case 'X': ++str; base = 16; break;    // Hexadecimal
                case 'o': case 'O': ++str; base = 8; break;     // Octal
                case 'b': case 'B': ++str; base = 2; break;     // Binary
            }
        }
        
        // Parse remaining digits in the determined base
        while(true) {
            if('_' == *str) ++str;  // Skip '_' separators
            
            digit = *str;
            if(16 == base) {
                // Hexadecimal: 0-9, a-f, A-F
                if('0' <= digit && '9' >= digit) digit -= '0';
                else if('a' <= digit && 'f' >= digit) digit -= 'a' - 10;
                else if('A' <= digit && 'F' >= digit) digit -= 'A' - 10;
                else break;
            }
            else {
                // Binary/octal/decimal: depends on base
                if('0' <= digit && '0' + base > digit) digit -= '0';
                else break;
            }
            
            val = val*base + digit;
            ++str;
        }
        return val;
    }
    
    /// Arena allocator storing all parsed Expression nodes. Capacity is 32 expressions per parse.
    FixedVector<Expression, 32> _arena;
};
