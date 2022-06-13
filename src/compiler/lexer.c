/*
   Copyright 2022 Alessandro Bozzato

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "lexer.h"

#define is_space(c) ((c) == ' ' || (c) == '\t')

typedef struct Lexer {
    FileId file_id;
    Token token;
    char const *str;
    char const *start;
    array(Diagnostic) diagnostics;
} Lexer;

typedef struct KeywordInfo {
    bool is_keyword;
    int start;
    int end;
} KeywordInfo;

typedef struct Keyword {
    char const *str;
    int length;
    TokenKind kind;
} Keyword;

#define KEYWORD(_ch, _first, _last) \
    [_ch] = { .is_keyword = true, .start = _first - TOKEN_FIRST_KEYWORD, .end = _last - TOKEN_FIRST_KEYWORD }
static KeywordInfo keyword_info[] = {
    KEYWORD('a', TOKEN_AS, TOKEN_ASM),
    KEYWORD('b', TOKEN_BREAK, TOKEN_BREAK),
    KEYWORD('c', TOKEN_CONTINUE, TOKEN_CONTEXT),
    KEYWORD('d', TOKEN_DEFER, TOKEN_DISTINCT),
    KEYWORD('e', TOKEN_ELSE, TOKEN_ENUM),
    KEYWORD('f', TOKEN_FOR, TOKEN_FALLTHROUGH),
    KEYWORD('i', TOKEN_IF, TOKEN_IMPORT),
    KEYWORD('m', TOKEN_MUT, TOKEN_MAP),
    KEYWORD('n', TOKEN_NEW, TOKEN_NEW),
    KEYWORD('o', TOKEN_OWN, TOKEN_OFFSETOF),
    KEYWORD('r', TOKEN_RETURN, TOKEN_RETURN),
    KEYWORD('s', TOKEN_STRUCT, TOKEN_SIZEOF),
    KEYWORD('t', TOKEN_TYPEOF, TOKEN_TYPEOF),
    KEYWORD('u', TOKEN_USING, TOKEN_UNDEF),
    KEYWORD('w', TOKEN_WHERE, TOKEN_WHEN),
};
#undef KEYWORD

#define X(_token, _str) { .str = _str, .length = sizeof(_str) - 1, .kind = _token },
static Keyword keywords[] = { KEYWORDS(X) };
#undef X

static int digit_decoder[128] = {
    ['1'] = 1,
    ['2'] = 2,
    ['3'] = 3,
    ['4'] = 4,
    ['5'] = 5,
    ['6'] = 6,
    ['7'] = 7,
    ['8'] = 8,
    ['9'] = 9,
    ['0'] = 0,
    ['a'] = 10,
    ['A'] = 10,
    ['b'] = 11,
    ['B'] = 11,
    ['c'] = 12,
    ['C'] = 12,
    ['d'] = 13,
    ['D'] = 13,
    ['e'] = 14,
    ['E'] = 14,
    ['f'] = 15,
    ['F'] = 15,
};

static bool is_escapeable[128] = { ['\\'] = true, ['\''] = true, ['"'] = true, ['0'] = true, ['t'] = true, ['v'] = true, ['r'] = true, ['n'] = true, ['b'] = true, ['a'] = true };

#define push_error(start, end, message, label, hint) (array_push(lexer->diagnostics, lexer_error(lexer, (start), (end), (message), (label), (hint))))

static Diagnostic lexer_error(Lexer *lexer, char const *start, char const *end, char const *message, char const *label, char const *hint)
{
    Span span = { .file_id = lexer->file_id, .start = start - lexer->start, .end = end - lexer->start };
    return error(span, message, label, hint);
}

static alwaysinline int lex_base(Lexer *lexer)
{
    int base = 10;

    if (*lexer->str != '0')
        return base;

    lexer->str++;
    if (*lexer->str == 'b' || *lexer->str == 'B') {
        lexer->str++;
        base = 2;
    } else if (*lexer->str == 'o' || *lexer->str == 'O') {
        lexer->str++;
        base = 8;
    } else if (*lexer->str == 'x' || *lexer->str == 'X') {
        lexer->str++;
        base = 16;
    } else {
        lexer->str--;
    }

    return base;
}

static int skip_digits(Lexer *lexer, int base)
{
    char const *str = lexer->str;
    int count = 0;

    while (true) {
        if (*str == '_') {
            str++;
            continue;
        }

        int digit = digit_decoder[(int)*str];

        if (digit == 0 && *str != '0')
            break;

        if (digit >= base) {
            if (*str == 'e' || *str == 'E')
                break;

            char const *label = strf("'%c' is not allowed in base '%d'", *str, base);
            push_error(str, str, "invalid digit in numeric literal", label, NULL);
            xfree(label);
        }

        str++;
        count++;
    }

    lexer->str = str;

    return count;
}

static flatten char const *lex_number(Lexer *lexer)
{
    assert(isdigit(*lexer->str));
    Token token = { .start = lexer->str, .kind = TOKEN_INT };

    int base = lex_base(lexer);

    int count = skip_digits(lexer, base);

    if (*lexer->str == '.') {
        lexer->str++;

        token.kind = TOKEN_FLOAT;

        skip_digits(lexer, base);

        if (base != 10 && base != 16) {
            char const *label = strf("this number is encoded in base '%d'", base);
            push_error(token.start, lexer->str, "invalid base in floating point literal", label, "Floating point literals support only two bases: 10 and 16");
            xfree(label);
        }

        if (base == 16) {
            // count should not be 0 nor anything bigger than 1
            if (count != 1) {
                char const *label;
                if (count == 0) {
                    label = strf("this number doesn't have the single digit after the 'x'");
                } else {
                    label = strf("this number has got too many digits after the 'x'");
                }
                push_error(token.start, lexer->str, "invalid hexadecimal floating point literal", label, "An hexadecimal floating point literal expects only one digit after the 'x'. For example:\n\n    0x1.2p2\n\n");
                xfree(label);
            }

            char const *p = lexer->str + 1;
            if (*p != 'p') {
                push_error(token.start, lexer->str, "invalid hexadecimal point literal", "this number misses the exponent", "An hexadecimal point literal always expects an exponent encoded in base 10");
            }
        }
    }

    if (*lexer->str == 'e' || *lexer->str == 'E') {
        token.kind = TOKEN_FLOAT;

        lexer->str++;

        int op = *lexer->str == '+' || *lexer->str == '-';
        lexer->str += op;

        skip_digits(lexer, 10);
    } else if (*lexer->str == 'p' || *lexer->str == 'P') {
        token.kind = TOKEN_FLOAT;

        if (base != 16) {
            // NOTE: probably this error should be rewritten
            char const *label = strf("this number is encoded in base '%d'", base);
            push_error(token.start, lexer->str, "invalid suffix", label, "The suffix 'p' is only used when the literal is in base 16. If you need scientific notation use either an hexadecimal floating point literal or a decimal floating point literal. For example:\n\n    hexadecimal: 0x1.2p2\n    decimal: 1.2e2\n\n");
            xfree(label);
        }

        lexer->str++;

        int op = *lexer->str == '+' || *lexer->str == '-';
        lexer->str += op;

        skip_digits(lexer, 10);
    }

    lexer->token = token;

    return lexer->str;
}

static char const *handle_escape(Lexer *lexer)
{
    assert(*lexer->str == '\\');
    char const *str = lexer->str;
    str++;

    if (*str == 'x') {
        str++;

        int digit = digit_decoder[(int)*str];
        if (digit == 0 && *str != '0') {
            char const *label = strf("invalid hex character '%c'");
            push_error(lexer->str, str, "invalid escape", label, NULL);
            xfree(label);
        } else {
            str++;

            digit = digit_decoder[(int)*str];
            if (digit != 0 || *str == '0') {
                str++;
            }
        }
    } else if (is_escapeable[(int)*str]) {
        str++;
    } else {
        char const *label = strf("invalid character '%c'", *str);
        push_error(lexer->str, str, "invalid escape", label, NULL);
        xfree(label);
    }

    return str;
}

static flatten inline char const *lex_char(Lexer *lexer)
{
    assert(*lexer->str == '\'');
    lexer->token.kind = TOKEN_CHAR;
    lexer->token.start = lexer->str;
    char const *str = lexer->str;

    // We assume we get called only from the main loop, so we can unconditionally increment the 'str' pointer
    str++;

    if (*str == '\\') {
        str = handle_escape(lexer);
    } else {
        str++;
    }

    if (likely(*str == '\'')) {
        str++;
    } else {
        push_error(str, str, "unterminated character literal", "add a quote here", NULL);

        // Skip characters until we finish the line/file
        while (*str && *str != '\n') {
            str++;
        }
    }

    return str;
}

static flatten char const *lex_string(Lexer *lexer)
{
    assert(*lexer->str == '"');
    Token token = { .start = lexer->str };
    char const *str = lexer->str;

    if (str[0] == '"' && str[1] == '"' && str[2] == '"') {
        str += 3;
        token.kind = TOKEN_MULTILINE_STRING;

        while (*str) {
            if (str[0] == '"' && str[1] == '"' && str[2] == '"') {
                str += 3;
                break;
            }

            if (*str == '\\') {
                str = handle_escape(lexer);
            } else {
                str++;
            }
        }
    } else {
        str++;
        token.kind = TOKEN_STRING;

        while (*str) {
            if (*str == '"' || *str == '\n') {
                str++;
                break;
            }

            if (*str == '\\') {
                str = handle_escape(lexer);
            } else {
                str++;
            }
        }
    }

    char const *quotes = str - 1;

    if (unlikely(*quotes != '"')) {
        char const *message;
        char const *hint;
        if (token.kind == TOKEN_STRING) {
            message = "unterminated string";
            hint = "Probably you forgot a \". Add it where it's needed.";
        } else {
            message = "unterminated multiline string";
            hint = "Probably you forgot three \". Add them where they're needed";
        }
        push_error(token.start, quotes, message, "missing \"", hint);
    }

    lexer->token = token;

    return str;
}

static char const *lex_identifier(Lexer *lexer)
{
    lexer->token.kind = TOKEN_IDENTIFIER;
    lexer->token.start = lexer->str;

    char const *str = lexer->str;

    while (utf8_isalnum(str) || *str == '_') {
        str += utf8_bytes(*str);
    }

    return str;
}

#define OP(_ch, _kind)             \
    case _ch: {                    \
        lexer->token.start = str;  \
        lexer->token.kind = _kind; \
        str++;                     \
    } break

#define OP_EQ(_ch, _kind)                   \
    case _ch: {                             \
        lexer->token.start = str;           \
        str++;                              \
        if (*str == '=') {                  \
            lexer->token.kind = _kind##_EQ; \
            str++;                          \
        } else {                            \
            lexer->token.kind = _kind;      \
        }                                   \
    } break;

#define OP2(_ch1, _kind1, _ch2, _kind2) \
    case _ch1: {                        \
        lexer->token.start = str;       \
        str++;                          \
        if (*str == _ch2) {             \
            lexer->token.kind = _kind2; \
            str++;                      \
        } else {                        \
            lexer->token.kind = _kind1; \
        }                               \
    } break;

#define OP2_EQ(_ch1, _kind1, _ch2, _kind2)   \
    case _ch1: {                             \
        lexer->token.start = str;            \
        str++;                               \
        if (*str == _ch2) {                  \
            lexer->token.kind = _kind2;      \
            str++;                           \
        } else if (*str == '=') {            \
            lexer->token.kind = _kind1##_EQ; \
            str++;                           \
        } else {                             \
            lexer->token.kind = _kind1;      \
        }                                    \
    } break;

#define OP3(_ch1, _kind1, _ch2, _kind2, _ch3, _kind3) \
    case _ch1: {                                      \
        lexer->token.start = str;                     \
        str++;                                        \
        if (*str == _ch2) {                           \
            lexer->token.kind = _kind2;               \
            str++;                                    \
        } else if (*str == _ch3) {                    \
            lexer->token.kind = _kind3;               \
            str++;                                    \
        } else {                                      \
            lexer->token.kind = _kind1;               \
        }                                             \
    } break;

#define OP3_EQ(_ch1, _kind1, _ch2, _kind2, _ch3, _kind3) \
    case _ch1: {                                         \
        lexer->token.start = str;                        \
        str++;                                           \
        if (*str == _ch2) {                              \
            lexer->token.kind = _kind2;                  \
            str++;                                       \
            if (*str == '=') {                           \
                lexer->token.kind = _kind2##_EQ;         \
                str++;                                   \
            }                                            \
        } else if (*str == _ch3) {                       \
            lexer->token.kind = _kind3;                  \
            str++;                                       \
        } else {                                         \
            lexer->token.kind = _kind1;                  \
        }                                                \
    } break;

/* clang-format off */
static flatten bool next_token(Lexer *lexer)
{
    char const *str;
repeat:
    str = lexer->str;
    switch (*str) {
    OP_EQ('+', TOKEN_PLUS);
    OP2_EQ('-', TOKEN_MINUS, '>', TOKEN_ARROW);
    OP_EQ('*', TOKEN_STAR);
    OP_EQ('%', TOKEN_PERCENTAGE);
    OP2_EQ('&', TOKEN_AND, '&', TOKEN_AND_AND);
    OP_EQ('^', TOKEN_CARET);
    OP3_EQ('<', TOKEN_LT, '<', TOKEN_LT_LT, '=', TOKEN_LT_EQ);
    OP3_EQ('>', TOKEN_GT, '>', TOKEN_GT_GT, '=', TOKEN_GT_EQ);
    OP3('=', TOKEN_EQ, '=', TOKEN_EQ_EQ, '>', TOKEN_FAT_ARROW);
    OP2('!', TOKEN_EXCLAMATION, '=', TOKEN_EXCLAMATION_EQ);
    OP('?', TOKEN_QUESTION);
    OP(',', TOKEN_COMMA);
    OP(';', TOKEN_SEMICOLON);
    OP(':', TOKEN_COLON);
    OP('@', TOKEN_AT);
    OP('~', TOKEN_TILDE);
    OP('(', TOKEN_LPAREN);
    OP(')', TOKEN_RPAREN);
    OP('[', TOKEN_LBRACKET);
    OP(']', TOKEN_RBRACKET);
    OP('{', TOKEN_LBRACE);
    OP('}', TOKEN_RBRACE);
    case '|': {
       lexer->token.start = str;
       str++;
       if (*str == '|') {
           lexer->token.kind = TOKEN_PIPE_PIPE;
           str++;
       } else if (*str == '=') {
           lexer->token.kind = TOKEN_PIPE_EQ;
           str++;
       } else if (*str == '>') {
           lexer->token.kind = TOKEN_PIPE_GT;
           str++;
       } else {
           lexer->token.kind = TOKEN_PIPE;
       }
    } break;
    case '.': {
        lexer->token.start = str;
        str++;
        if (*str != '.') {
            lexer->token.kind = TOKEN_DOT;
        } else {
            str++;
            if (*str != '.') {
                lexer->token.kind = TOKEN_DOT_DOT;
            } else {
                str++;
                lexer->token.kind = TOKEN_ELLIPSIS;
            }
        }
    } break;
    case '/': {
        lexer->token.start = str;
        str++;
        if (*str == '/') {
            str++;

            TokenKind kind = TOKEN_COMMENT;
            if (*str == '/') {
                str++;
                kind = TOKEN_DOC_COMMENT;
            }

            while (*str != '\r' && *str != '\n') {
                str++;
            }
            str += *str == '\r';

            lexer->token.kind = kind;
        } else if (*str == '*') {
            str++;

            int depth = 1;
            while (*str && depth > 0) {
                if (str[0] == '/' && str[1] == '*') {
                    str += 2;
                    depth++;
                } else if (str[0] == '*' && str[1] == '/') {
                    str += 2;
                    depth--;
                } else {
                    str++;
                }
            }

            lexer->token.kind = TOKEN_MULTILINE_COMMENT;
        } else if (*str == '=') {
            str++;
            lexer->token.kind = TOKEN_SLASH_EQ;
        } else {
            lexer->token.kind = TOKEN_SLASH;
        }
    } break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9': {
        str = lex_number(lexer);
    } break;
    case '\'': {
        str = lex_char(lexer);
    } break;
    case '"': {
        str = lex_string(lexer);
    } break;
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i': case 'j':
    case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q': case 'r': case 's': case 't':
    case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I': case 'J':
    case 'K': case 'L': case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T':
    case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
    case '_': {
        lexer->token.start = lexer->str;
        str = lex_identifier(lexer);

        int length = str - lexer->str;
        TokenKind kind = TOKEN_IDENTIFIER;

        if (length <= max_keyword_length && keyword_info[(int)*lexer->str].is_keyword) {
            KeywordInfo info = keyword_info[(int)*lexer->str];

            for (int i = info.start; i <= info.end; ++i) {
                Keyword keyword = keywords[i];

                if (length == keyword.length && strncmp(lexer->str, keyword.str, length) == 0) {
                    kind = keyword.kind;
                    break;
                }
            }
        }

        lexer->token.kind = kind;
    } break;
    case '\r':
        str++;
    case '\n': {
        lexer->token.start = str;
        lexer->token.kind = TOKEN_NEWLINE;
        str++;
    } break;
    case ' ':
    case '\t': {
        while (is_space(*lexer->str)) {
            lexer->str++;
        }
        goto repeat;
    } break;
    case '\0': {
        lexer->token.kind = TOKEN_EOF;
    } break;
    default: {
        if (utf8_isalpha(str)) {
            lexer->token.start = str;
            lexer->token.kind = TOKEN_IDENTIFIER;
            while (utf8_isalnum(str)) {
                str += utf8_bytes(*str);
            }
        } else {
            char const *message = strf("unknown character '%c'", *str);
            push_error(str, str, message, "", NULL);
            xfree(message);

            lexer->token.start = str;
            lexer->token.kind = TOKEN_BAD;
            str++;
        }
    } break;
    }

    lexer->str = str;
    return lexer->token.kind != TOKEN_EOF;
}
/* clang-format on */

#undef OP
#undef OP_EQ
#undef OP2
#undef OP2_EQ
#undef OP3
#undef OP3_EQ

LexedSrc lex(FileId file_id, stringview src)
{
    Lexer lexer = { .file_id = file_id, .str = src.str, .start = src.str };

    LexedSrc result = { .src = src };

    // Reserve space for the arrays
    size_t size = src.length / 8;
    array_reserve(result.kind, size);
    array_reserve(result.start, size);

    // Actually lex the string
    const char *start = src.str;
    while (next_token(&lexer)) {
        array_push(result.kind, lexer.token.kind);
        array_push(result.start, (uint32_t)(lexer.token.start - start));
    }

    array_push(result.kind, TOKEN_EOF);
    array_push(result.start, src.length);

    result.num_tokens = array_length(result.kind);

    return result;
}

char const *token_to_string(TokenKind kind)
{
#define X(_token, _str) [_token] = _str,
    static char const *strings[] = { TOKENS(X) };
#undef X
    return strings[kind];
}

int token_length(Token token)
{
    if (token.kind == TOKEN_INT || token.kind == TOKEN_FLOAT) {
        Lexer lexer = { .str = token.start };
        char const *end = lex_number(&lexer);
        return end - token.start;
    } else if (token.kind == TOKEN_CHAR) {
        Lexer lexer = { .str = token.start };
        char const *end = lex_char(&lexer);
        return end - token.start;
    } else if (token.kind == TOKEN_STRING || token.kind == TOKEN_MULTILINE_STRING) {
        Lexer lexer = { .str = token.start };
        char const *end = lex_string(&lexer);
        return end - token.start;
    } else if (token.kind == TOKEN_IDENTIFIER) {
        Lexer lexer = { .str = token.start };
        char const *end = lex_identifier(&lexer);
        return end - token.start;
    } else {
#define X(_token, _str) [_token] = sizeof(_str),
        static int lengths[] = { TOKENS(X) };
#undef X
        return lengths[token.kind];
    }
}
