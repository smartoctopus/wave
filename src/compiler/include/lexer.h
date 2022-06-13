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

#ifndef LEXER_H_
#define LEXER_H_

#include <assert.h> // assert()

#include "diagnostic.h" // error(), warn()
#include "util.h"       // array(), stringview, utf8_isalpha(), utf8_isalnum()
#include "vfs.h"        // FileId

/// x-macro to generate both the enum and the array of printable names
/// NOTE: the enum variants should describe the name of the written character
///       e.g. | => pipe (and not or)
#define TOKENS(X)                                   \
    X(TOKEN_EOF, "EOF")                             \
    X(TOKEN_BAD, "unknown character")               \
    X(TOKEN_COMMENT, "comment")                     \
    X(TOKEN_DOC_COMMENT, "docs comment")            \
    X(TOKEN_MULTILINE_COMMENT, "multiline comment") \
    X(TOKEN_INT, "an int literal")                  \
    X(TOKEN_FLOAT, "a float literal")               \
    X(TOKEN_CHAR, "a char literal")                 \
    X(TOKEN_STRING, "a string literal")             \
    X(TOKEN_MULTILINE_STRING, "a multiline string") \
    X(TOKEN_IDENTIFIER, "an identifier")            \
    X(TOKEN_LPAREN, "(")                            \
    X(TOKEN_RPAREN, ")")                            \
    X(TOKEN_LBRACKET, "[")                          \
    X(TOKEN_RBRACKET, "]")                          \
    X(TOKEN_LBRACE, "{")                            \
    X(TOKEN_RBRACE, "}")                            \
    X(TOKEN_AT, "@")                                \
    X(TOKEN_EXCLAMATION, "!")                       \
    X(TOKEN_TILDE, "~")                             \
    X(TOKEN_QUESTION, "?")                          \
    X(TOKEN_STAR, "*")                              \
    X(TOKEN_SLASH, "/")                             \
    X(TOKEN_PERCENTAGE, "%")                        \
    X(TOKEN_AND, "&")                               \
    X(TOKEN_LT_LT, "<<")                            \
    X(TOKEN_GT_GT, ">>")                            \
    X(TOKEN_PLUS, "+")                              \
    X(TOKEN_MINUS, "-")                             \
    X(TOKEN_PIPE, "|")                              \
    X(TOKEN_CARET, "^")                             \
    X(TOKEN_EQ_EQ, "==")                            \
    X(TOKEN_EXCLAMATION_EQ, "!=")                   \
    X(TOKEN_LT, "<")                                \
    X(TOKEN_GT, ">")                                \
    X(TOKEN_LT_EQ, "<=")                            \
    X(TOKEN_GT_EQ, ">=")                            \
    X(TOKEN_AND_AND, "&&")                          \
    X(TOKEN_PIPE_PIPE, "||")                        \
    X(TOKEN_PIPE_GT, "|>")                          \
    X(TOKEN_EQ, "=")                                \
    X(TOKEN_COLON_EQ, ":=")                         \
    X(TOKEN_STAR_EQ, "*=")                          \
    X(TOKEN_SLASH_EQ, "/=")                         \
    X(TOKEN_PERCENTAGE_EQ, "%=")                    \
    X(TOKEN_AND_EQ, "&=")                           \
    X(TOKEN_LT_LT_EQ, "<<=")                        \
    X(TOKEN_GT_GT_EQ, ">>=")                        \
    X(TOKEN_PLUS_EQ, "+=")                          \
    X(TOKEN_MINUS_EQ, "-=")                         \
    X(TOKEN_PIPE_EQ, "|=")                          \
    X(TOKEN_CARET_EQ, "^=")                         \
    X(TOKEN_ARROW, "->")                            \
    X(TOKEN_FAT_ARROW, "=>")                        \
    X(TOKEN_COMMA, ",")                             \
    X(TOKEN_DOT, ".")                               \
    X(TOKEN_DOT_DOT, "..")                          \
    X(TOKEN_ELLIPSIS, "...")                        \
    X(TOKEN_COLON, ":")                             \
    X(TOKEN_COLON_COLON, "::")                      \
    X(TOKEN_SEMICOLON, ";")                         \
    X(TOKEN_NEWLINE, "NEWLINE")                     \
    KEYWORDS(X)

#define KEYWORDS(X)                     \
    X(TOKEN_FIRST_KEYWORD, "")          \
    X(TOKEN_AS, "as")                   \
    X(TOKEN_ALIGNOF, "alignof")         \
    X(TOKEN_ASM, "asm")                 \
    X(TOKEN_BREAK, "break")             \
    X(TOKEN_CONTINUE, "continue")       \
    X(TOKEN_CONTEXT, "context")         \
    X(TOKEN_DEFER, "defer")             \
    X(TOKEN_DISTINCT, "distinct")       \
    X(TOKEN_ELSE, "else")               \
    X(TOKEN_ENUM, "enum")               \
    X(TOKEN_FOR, "for")                 \
    X(TOKEN_FOREIGN, "foreign")         \
    X(TOKEN_FALLTHROUGH, "fallthrough") \
    X(TOKEN_IF, "if")                   \
    X(TOKEN_IN, "in")                   \
    X(TOKEN_IMPORT, "import")           \
    X(TOKEN_MUT, "mut")                 \
    X(TOKEN_MATCH, "match")             \
    X(TOKEN_MAP, "map")                 \
    X(TOKEN_NEW, "new")                 \
    X(TOKEN_OWN, "own")                 \
    X(TOKEN_OR, "or")                   \
    X(TOKEN_OFFSETOF, "offsetof")       \
    X(TOKEN_RETURN, "return")           \
    X(TOKEN_STRUCT, "struct")           \
    X(TOKEN_SIZEOF, "sizeof")           \
    X(TOKEN_TYPEOF, "typeof")           \
    X(TOKEN_USING, "using")             \
    X(TOKEN_UNION, "union")             \
    X(TOKEN_UNDEF, "undef")             \
    X(TOKEN_WHERE, "where")             \
    X(TOKEN_WHEN, "when")

#define max_keyword_length 11

typedef enum TokenKind {
#define X(_token, _lit) _token,
    TOKENS(X)
#undef X
} TokenKind;

typedef struct Token {
    TokenKind kind;
    char const *start;
} Token;

typedef struct LexedSrc {
    stringview src;
    size_t num_tokens;
    array(TokenKind) kind;
    array(uint32_t) start;
} LexedSrc;

/// Lex a string with an associated 'file_id'
LexedSrc lex(FileId file_id, stringview src);

/// Return the string representation of the token
char const *token_to_string(TokenKind kind);

/// Return the length of the token
int token_length(Token token);

#endif // LEXER_H_
