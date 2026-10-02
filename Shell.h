#pragma once

#include "string_view.h"
#include "fixed_vector.h"
#include "fixed_map.h"
#include "builtin.h"
#include "Evaluator.h"

#include "USBSerial.h"

// Minimal Python-like REPL (Read-Eval-Print Loop) shell for Pokitto
//
// Implements an interactive command-line interface that allows users to:
// - Execute basic arithmetic and logic operations
// - Define and use variables during the session
// - Navigate command history using arrow keys
// - Access memory and peripheral registers through built-in arrays (mem8, mem16, mem32)
//
// Design considerations:
// - Uses FixedVector/FixedMap to avoid dynamic memory allocation
// - All buffers have compile-time size limits

class Shell {
    
    public:
    
    /**
     * Construct a new Shell instance with the provided serial interface
     */
    explicit Shell(USBSerial* serial) : _serial(serial), _line{}, _cursor_pos(0), _history(), _history_index(0), _vars(), _builtins(), _eval() {
        
        // Register built-in memory access functions:
        // - mem32: Returns pointer for 32-bit access, align address to 4-bytes
        _builtins[StringView("mem32")] = Builtin::subscript_op(
            [](unsigned int i) { return reinterpret_cast<void*>(i&~3); }, 4 );
        // - mem16: Returns pointer for 16-bit access, align address to 2-bytes
        _builtins[StringView("mem16")] = Builtin::subscript_op(
            [](unsigned int i) { return reinterpret_cast<void*>(i&~1); }, 2 );
        // - mem8: Returns pointer for 8-bit access
        _builtins[StringView("mem8")] = Builtin::subscript_op(
            [](unsigned int i) { return reinterpret_cast<void*>(i); }, 1 );
        
        // Command line buffer initialized with null terminator
        _line.push_back('\0');
        
        _serial->attach(this, &Shell::process);
        
        // Calculate and display the user data memory region
        const auto p = reinterpret_cast<uintptr_t>(_user_data);
        _serial->printf("Minimal Python-like REPL for Pokitto\r\nUser data: 0x%lx to 0x%lx\r\n", p, p + sizeof(_user_data) - 1);
        
        // Display the initial command prompt
        _serial->printf(">>> ");
    }
    
    /**
     * Process incoming serial input
     */
    void process() {
        // Return immediately if no input data waiting
        while(_serial->available() > 0) {
            // Read next character from serial interface
            int ch = _serial->_getc();
            
            if(ch == 27) {
                // Handle ANSI escape sequences (arrow keys, del, home, end)
                _handle_escape_input();
            }
            else {
                // Handle regular character input
                _handle_input(ch);
            }
        }
    }
    
    
    private:
    
    /**
     * Process regular (non-escape) character input
     */
    void _handle_input(int ch) {
        if(ch >= 32 && ch <= 126) {         // Regular printable characters
            _insert_char(ch);
        }
        else if(ch == 8 || ch == 127) {     // Backspace
            if(_cursor_pos > 0) {
                _serial->printf("\b");      // Move terminal cursor back one position
                --_cursor_pos;
                _erase_char();
            }
        }
        else if(ch == 10 || ch == 13) {     // Newline (LF) or carriage Return (CR) / Enter
            _serial->printf("\r\n");        // Terminate current line visually
            _execute_command();             // Parse and evaluate the command
        }
        // Ignore other control characters
    }
    
    /**
     * Handle ANSI escape sequences
     */
    void _handle_escape_input() {
        int second_byte = _serial->_getc(); // Read second byte of escape sequence
        if(second_byte == '[') {            // '[' indicates CSI (Control Sequence Introducer)
            _handle_terminal_key();
        }
        // Ignore other escape codes
    }
    
    /**
     * Process terminal special keys (arrow keys, del, home, end)
     */
    void _handle_terminal_key() {
        int ch = _serial->_getc();
        int param_byte = '\0';
        if(ch < 0x40 || ch > 0x7e) {
            param_byte = ch;
            while(ch < 0x40 || ch > 0x7e) {
                ch = _serial->_getc();
            }
        }
        
        switch(ch) {
            case 'A': _navigate_history(-1); break; // Up arrow (↑)
            case 'B': _navigate_history(1); break;  // Down arrow (↓)
            case 'C': _move_cursor_right(); break;  // Right arrow (→)
            case 'D': _move_cursor_left(); break;   // Left arrow (←)
            case 'H': _move_cursor_home(); break;   // Home
            case 'F': _move_cursor_end(); break;    // End
            case '~':
                if(param_byte == '1') _move_cursor_home();      // Home
                else if(param_byte == '3') _erase_char();       // Del
                else if(param_byte == '4') _move_cursor_end();  // End
                break;
            default: break;                         // Unkown key - ignore
        }
    }
    
    /**
     * Insert a character at the current cursor position
     */
    void _insert_char(char ch) {
        // Insert character at current cursor position (shifts existing chars right)
        _line.insert(_line.begin() + _cursor_pos, 1, ch);
        
        // Calculate current visible line length (excluding null terminator)
        const auto len = _line.size() > 0 ? _line.size() - 1 : 0;
        
        // Print all characters from insertion point to end of line
        _serial->printf("%s", &_line[_cursor_pos]);
        
        // Update cursor position after the inserted character
        if(_cursor_pos < len) {
            ++_cursor_pos;
            // If not at end of line, move cursor back to correct position
            if(_cursor_pos < len) _serial->printf("\x1b[%dD", static_cast<int>(len - _cursor_pos));
        }
    }
    
    /**
     * Erase the character at the current cursor position
     */
    void _erase_char() {
        // Calculate current visible line length (excluding null terminator)
        auto len = _line.size() > 0 ? _line.size() - 1 : 0;
        
        // Only erase if not at the end of line
        if(_cursor_pos < len) {
            const auto pos = _line.begin() + _cursor_pos;
            _line.erase(pos, pos + 1);    // Remove character from line buffer
            --len;                        // Decrease line length counter
            
            // Clear everything from cursor to end of line visually
            _serial->printf("\x1b[K");
            
            // If there's still content after the deletion point, redraw it
            if(_cursor_pos < len) {
                _serial->printf("%s", &_line[_cursor_pos]);             // Print remaining line content
                _serial->printf("\x1b[%dD", (int)(len - _cursor_pos));  // Reposition cursor
            }
        }
    }
    
    /**
    /* Parse, evaluate, and execute the current command line
     */
    void _execute_command() {
        // Skip processing for empty lines (only contains null terminator)
        if(1 < _line.size()) {
            // Store command in history if it's not a duplicate of the most recent entry
            if(_history.empty() || StringView(_history.back().begin()) != StringView(_line.begin())) {
                if(_history.size() >= MAX_HISTORY) {
                    // If history is full, remove oldest entry
                    _history.erase(_history.begin(), _history.begin() + 1);
                }
                _history.push_back(_line);  // Add to back (newest last)
            }
            
            // Attempt to parse the command line as an expression
            const char* error = nullptr;
            auto expr = _eval.parse(&_line[0], error);
            
            if(!expr) {
                // Syntax error occurred during parsing
                _serial->printf("SyntaxError: %s\r\n", error);
            }
            else if(Evaluator::Expression::Type::EMPTY != expr->type) {
                // Successfully parsed a non-empty expression
                
                // Attempt to evaluate the expression
                error = nullptr;
                auto val = _eval.evaluate(expr, error, _vars, _builtins);
                
                if(error) {
                    // Runtime error occurred during evaluation
                    _serial->printf("RuntimeError: %s\r\n", error);
                }
                else if(Evaluator::Expression::Type::ASSIGN != expr->type) {
                    // Successfully evaluated expression
                    
                    // Store result in underscore variable (convention for last result)
                    _vars['_'] = val;
                    
                    // Display the result value
                    _serial->printf("%ld\r\n", val);
                }
                // Assignment statements don't produce output
            }
        }
        
        _line.clear();          // Reset line buffer for next command
        _line.push_back('\0');  // Maintain null termination
        _cursor_pos = 0;        // Reset cursor to beginning
        _history_index = _history.size();     // Reset history navigation index
        
        // Display fresh prompt for next command
        _serial->printf(">>> ");
    }
    
    /**
     * Navigate through command history
     */
    void _navigate_history(int direction) {
        auto new_index = _history_index;
        if(direction > 0) {
            // Move to a newer entry in history
            if(_history_index < _history.size()) ++new_index;
        }
        else if(direction < 0) {
            // Move to an older entry in history
            if(_history_index > 0) --new_index;
        }
        
        if(new_index == _history_index) return;
        
        _history_index = new_index;
        if(_history_index < _history.size()) {
            // Update line buffer content from history
            _line = _history[_history_index];
        }
        else {
            // Reached end of history - reset line buffer to empty string
            _line.clear();
            _line.push_back('\0');
        }
        
        if(0 < _cursor_pos) {
            // Move cursor to beginning of line
            _serial->printf("\x1b[%dD", _cursor_pos);
        }
        // Clear from cursor to end, then print the recalled command
        _serial->printf("\x1b[K%s", &_line[0]);
        
        // Position cursor at end of command line
        _cursor_pos = _line.size() > 0 ? _line.size() - 1 : 0;
    }
    
    inline void _move_cursor_right() {
        const auto len = _line.size() > 0 ? _line.size() - 1 : 0;
        if(len > _cursor_pos) {
            _serial->putc(_line[_cursor_pos]);  // Echo character at cursor position
            ++_cursor_pos;                      // Advance logical cursor position
        }
    }
    inline void _move_cursor_left() {
        if(0 < _cursor_pos) {
            _serial->printf("\b");              // Terminal backspace
            --_cursor_pos;                      // Decrement logical cursor position
        }
    }
    inline void _move_cursor_home() {
        if(0 < _cursor_pos) {
            // Move cursor left '_cursor_pos' times
            _serial->printf("\x1b[%dD", _cursor_pos);
            _cursor_pos = 0;                    // Reset logical cursor position to start
        }
    }
    inline void _move_cursor_end() {
        const auto len = _line.size() > 0 ? _line.size() - 1 : 0;
        if(len > _cursor_pos) {
            // Move cursor right 'len - _cursor_pos' times
            _serial->printf("\x1b[%dC", (int)(len - _cursor_pos));
            _cursor_pos = len;                  // Set logical cursor position to end of line
        }
    }
    
    // Maximum allowed line length in characters
    static constexpr std::size_t MAX_LINE_LEN = 96;
    
    // Maximum number of commands retained in history buffer
    static constexpr std::size_t MAX_HISTORY = 32;
    
    // Serial communication interface
    USBSerial* _serial;
    
    // Current command line buffer including null terminator
    FixedVector<char, MAX_LINE_LEN + 1> _line;
    
    // Cursor position within current line (0 = before first character)
    int _cursor_pos;
    
    // Command history stack (most recent commands last)
    FixedVector<FixedVector<char, MAX_LINE_LEN + 1>, MAX_HISTORY> _history;
    int _history_index;
    
    // Storage for user-defined variables during session. Maps single-character names to integer values.
    FixedMap<char, long int, 53> _vars;
    
    // Built-in functions and constants. Maps string names to Builtin function pointers.
    FixedMap<StringView, Builtin, 5> _builtins;
    
    // Expression parser and evaluator instance. Handles syntax parsing and computation of expressions.
    Evaluator _eval;
    
    // Reserve memory region for user data during session (1024-byte aligned)
    inline static uint8_t _user_data[0x1000] __attribute__ ((aligned(0x400)));
};
