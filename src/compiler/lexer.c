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

            // FIXME: This is temporarly a fprintf call. Change it to our own diagnostic API
            fprintf(stderr, "Digit '%c' is not allowed in base '%d'\n", *str, base);
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

        if (base != 10 && base != 16) {
            // FIXME: This is temporarly a fprintf call. Change it to our own diagnostic API
            fprintf(stderr, "Invalid base %d in floating point literal\n", base);
        }

        if (base == 16 && count > 1) {
            // FIXME: This is temporarly a fprintf call. Change it to our own diagnostic API
            fprintf(stderr, "Invalid base 16 floating point literal\n");
        }

        skip_digits(lexer, base);
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
            // FIXME: This is temporarly a fprintf call. Change it to our own diagnostic API
            fprintf(stderr, "Invalid 'p' suffix with base %d\n", base);
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
            // FIXME: This is temporarly a fprintf call. Change it to our own diagnostic API
            fprintf(stderr, "Invalid hex character '%c' in character literal\n", *str);
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
        fprintf(stderr, "Invalid character '%c' after escape\n", *str);
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
        // FIXME: This is temporarly a fprintf call. Change it to our own diagnostic API
        fprintf(stderr, "Unterminated character literal\n");
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
            if (*str == '"') {
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
        fprintf(stderr, "Unterminated string\n");
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
            fprintf(stderr, "Unknown character '%c'\n", *str);
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
    Lexer lexer = { .file_id = file_id, .str = src.str };

    LexedSrc result = { .src = src };

    // Reserve space for the arrays
    size_t size = src.length;
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
