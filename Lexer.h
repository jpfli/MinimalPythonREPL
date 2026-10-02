#pragma once

// Minimal, allocation-free, single-pass tokenizer for a small Python-like expression language
//
// Supported tokens:
//   - END_OF_LINE : pseudo-token marking the terminating NUL character
//   - NUMBER      : integers with optional 0x/0o/0b prefixes and '_' separator
//   - IDENTIFIER  : [A-Za-z_][A-Za-z0-9_]*
//   - ASSIGN      : =
//   - Brackets    : [ ]
//   - Parentheses : ( )
//   - Operators   : + - * / ~ & | ^
//   - COMMENT     : '#' up to end of line (skipped, never emitted)
//   - ILLEGAL     : fallback token for characters not matching anything else
//
// Design notes:
//   - The lexer maintains two pointers into the input string:
//       _cur   : start of the currently lexed token
//       _next  : one-past-the-end of that token
//   - Each matcher receives the cursor BY REFERENCE and, on success, leaves it
//     advanced past the matched text; on failure it must leave it untouched.

class Lexer {

    public:
    
    using iterator = const char*;
    
    
    /**
     * Enumeration of all token kinds the lexer can produce
     */
    enum class Token { END_OF_LINE, NUMBER, IDENTIFIER, ASSIGN, LBRACKET, RBRACKET, LPAREN, RPAREN, PLUS, MINUS, MUL, DIV, NOT, AND, OR, XOR, ILLEGAL };
    
    /**
     * Construct a lexer over `str`
     */
    Lexer(const char* str) : _cur(str), _next(str), _token(Token::ILLEGAL) {
        consume();      // Lex the first token, so `peek()` is valid as soon as the object exists
    }
    
    /**
     * Pointer to the first character of the current token (in the original
     * input string). Valid until the next call to consume()
     */
    const char* token_pos() const {
        return _cur;
    }
    
    /**
     * Length of the current token
     */
    const std::size_t token_len() const {
        return _next - _cur;
    }
    
    /**
     * Look at the current token without advancing
     */
    const Token& peek() const {
        return _token;
    }
    
    /**
     * Advance to the next token
     */
    void consume() {
        // Whitespace and comments are skipped
        while(_match_whitespace(_next) || _match_comment(_next));
        
        // _cur is pulled forward so it always points at the start of real content
        _cur = _next;
        _token = Token::ILLEGAL;
        
        // Try each token type in order - Matcher advances _next on success
        if(_match_integer(_next)) _token = Token::NUMBER;
        else if(_match_identifier(_next)) _token = Token::IDENTIFIER;
        else if(_match_assign(_next)) _token = Token::ASSIGN;
        else if(_match_lbracket(_next)) _token = Token::LBRACKET;
        else if(_match_rbracket(_next)) _token = Token::RBRACKET;
        else if(_match_lparen(_next)) _token = Token::LPAREN;
        else if(_match_rparen(_next)) _token = Token::RPAREN;
        else if(_match_plus(_next)) _token = Token::PLUS;
        else if(_match_minus(_next)) _token = Token::MINUS;
        else if(_match_mul(_next)) _token = Token::MUL;
        else if(_match_div(_next)) _token = Token::DIV;
        else if(_match_invert(_next)) _token = Token::NOT;
        else if(_match_and(_next)) _token = Token::AND;
        else if(_match_or(_next)) _token = Token::OR;
        else if(_match_xor(_next)) _token = Token::XOR;
        else if('\0' == *_next) _token = Token::END_OF_LINE;
        else ++_next;  // No match: swallow one char as ILLEGAL
    }
    
    /**
     * If the current token matches `token`, advance and return true
     */
    bool consume_if(const Token& token) {
        if(token == _token) {
            consume();
            return true;
        }
        return false;
    }
    
    
    private:
    
    /**
     * Matcher: Integer literal with optional 0x/0o/0b prefixes - may contain '_' as a visual separator
     * between digits, but the literal must not end with a separator
     */
    constexpr static bool _match_integer(iterator& pos) {
        // Must start with a decimal digit
        if('0' > *pos || '9' < *pos) return false;
        
        // Remember where the number started so we can roll back on failure
        const iterator start = pos;
        
        // If the loop exits with match == false, the whole match is rejected
        bool match = false;
        
        int base = 10;
        if('0' == *pos) {
            ++pos;
            // Leading '0' introduces an optional radix prefix
            switch(*pos) {
                case 'x': case 'X': ++pos; base = 16; break;    // Hexadecimal
                case 'o': case 'O': ++pos; base = 8; break;     // Octal
                case 'b': case 'B': ++pos; base = 2; break;     // Binary
                default:
                    // Special handling for decimal integers with leading zero - Must be all zeros
                    match = true;
                    while(true) {
                        if('_' == *pos) {
                            match = false;  // Separator clears match - It is set again on the next valid zero digit
                            ++pos;
                        }
                        if('0' != *pos) {
                            // Any non-zero digit invalidates the match
                            if('0' <= *pos && '9' >= *pos) match = false;
                            break;
                        }
                        match = true;
                        ++pos;
                    }
                    
                    // Roll back if there was no match
                    if(!match) pos = start;
                    
                    return match;
            }
        }
        
        while(true) {
            if('_' == *pos) {
                match = false;  // Separator clears match - It is set again on the next valid digit
                ++pos;
            }
            
            if(16 == base) {    // Hexadecimal: accept 0-9, a-f, A-F
                if(('0' > *pos || '9' < *pos) && ('a' > *pos || 'f' < *pos) && ('A' > *pos || 'F' < *pos)) break;
            }
            else {              // Decimal/octal/binanry
                if('0' > *pos || '0'+base <= *pos) break;
            }
            
            match = true;       // Got a valid digit for the base
            ++pos;
        }
        
        // Roll back if there was no match
        if(!match) pos = start;
        
        return match;
    }
    
    /**
     * Matcher: identifier, e.g. variable and function names
     */
    constexpr static bool _match_identifier(iterator& pos) {
        // First character must be a letter or underscore
        if(('a' > *pos || 'z' < *pos) && ('A' > *pos || 'Z' < *pos) && '_' != *pos) return false;
        ++pos;
        // Subsequent characters may additionally include digits
        while(('a' <= *pos && 'z' >= *pos) || ('A' <= *pos && 'Z' >= *pos) || ('0' <= *pos && '9' >= *pos) || '_' == *pos) {
            ++pos;
        }
        return true;
    }
    
    // ========================================================================
    // Single-character matchers:
    //   - If the char at 'pos' differs, return false without touching pos
    //   - On match advance 'pos' by one and return true
    // ========================================================================
    
    constexpr static bool _match_assign(iterator& pos) {
        if('=' != *pos) return false;
        ++pos;
        return true;
    }
    
    constexpr static bool _match_lbracket(iterator& pos) {
        if('[' != *pos) return false;
        ++pos;
        return true;
    }
    
    constexpr static bool _match_rbracket(iterator& pos) {
        if(']' != *pos) return false;
        ++pos;
        return true;
    }
    
    constexpr static bool _match_lparen(iterator& pos) {
        if('(' != *pos) return false;
        ++pos;
        return true;
    }
    
    constexpr static bool _match_rparen(iterator& pos) {
        if(')' != *pos) return false;
        ++pos;
        return true;
    }
    
    constexpr static bool _match_invert(iterator& pos) {
        if('~' != *pos) return false;
        ++pos;
        return true;
    }
    
    constexpr static bool _match_plus(iterator& pos) {
        if('+' != *pos) return false;
        ++pos;
        return true;
    }
    
    constexpr static bool _match_minus(iterator& pos) {
        if('-' != *pos) return false;
        ++pos;
        return true;
    }
    
    constexpr static bool _match_or(iterator& pos) {
        if('|' != *pos) return false;
        ++pos;
        return true;
    }
    
    constexpr static bool _match_xor(iterator& pos) {
        if('^' != *pos) return false;
        ++pos;
        return true;
    }
    
    constexpr static bool _match_and(iterator& pos) {
        if('&' != *pos) return false;
        ++pos;
        return true;
    }
    
    constexpr static bool _match_mul(iterator& pos) {
        if('*' != *pos) return false;
        ++pos;
        return true;
    }
    
    constexpr static bool _match_div(iterator& pos) {
        if('/' != *pos) return false;
        ++pos;
        return true;
    }
    
    /**
     * Skip a run of whitespace (space, tab, newline, carriage return).
     * Returns true if at least one character was skipped, false otherwise.
     */
    constexpr static bool _match_whitespace(iterator& pos) {
        if(' ' != *pos && '\t' != *pos && '\n' != *pos && '\r' != *pos) return false;
        ++pos;
        while(' ' == *pos || '\t' == *pos || '\n' == *pos || '\r' == *pos) {
            ++pos;
        }
        return true;
    }
    
    /**
     * Skip a comment: everything from '#' up to the end of the line.
     * Returns true if comment was found, false otherwise.
     */
    constexpr static bool _match_comment(iterator& pos) {
        if('#' != *pos) return false;
        ++pos;
        while('\n' != *pos && '\r' != *pos && '\0' != *pos) {
            ++pos;
        }
        return true;
    }
    
    iterator _cur;      // Start of the current token within the input
    iterator _next;     // One past the end of the current token
    Token _token;       // Result of the last match
};
