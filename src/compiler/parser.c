#include "parser.h"
#include "ast.h"

// Token macros
#define peek(_n) (parser->token_kind[parser->token_index + (_n)])
#define current() peek(0)
#define advance() (parser->token_index++)
#define index() (parser->token_index)

#define match(_kind) (__match(parser, (_kind)))
#define expect(_kind, _hint) (__expect(parser, (_kind), (_hint)))

/// Internal data structure used to parse a source string
typedef struct Parser {
    // Parsing
    NodeList nodes;
    array(Index) decls;

    // Lexed source string
    array(TokenKind) token_kind;
    array(uint32_t) token_start;
    size_t num_tokens;

    // Diagnostics
    array(Diagnostic) diagnostics;

    // The string which we are parsing
    char const *str;

    // Index in the tokens arrays
    uint32_t token_index;
    // File we are parsing
    FileId file_id;
} Parser;

enum { invalid = 0 };

static Index add_node(Parser *parser, NodeKind kind, Index token, Data data)
{
    array_push(parser->nodes.kind, kind);
    array_push(parser->nodes.token, token);
    array_push(parser->nodes.data, data);
    return array_length(parser->nodes.kind) - 1;
}

static alwaysinline Index reserve_node(Parser *parser)
{
    return add_node(parser, NODE_INVALID, 0, (Data) { 0 });
}

static void set_node(Parser *parser, Index index, NodeKind kind, Index token, Data data)
{
    parser->nodes.kind[index] = kind;
    parser->nodes.token[index] = token;
    parser->nodes.data[index] = data;
}

static bool __match(Parser *parser, TokenKind kind)
{
    if (current() == kind) {
        advance();
        return true;
    }
    return false;
}

static void error_at_current(Parser *parser, char const *message, char const *label, char const *hint)
{
    size_t start = parser->token_start[parser->token_index];
    Token token = { .start = parser->str + start, .kind = current() };
    Span span = {
        .file_id = parser->file_id,
        .start = start,
        .end = start + token_length(token),
    };

    Diagnostic diag = error(span, message, label, hint);
    array_push(parser->diagnostics, diag);
}

static bool __expect(Parser *parser, TokenKind kind, char const *hint)
{
    if (current() == kind) {
        advance();
        return true;
    }
    char const *message = strf("expected '%s'", token_to_string(kind));
    char const *label = strf("found '%s'", token_to_string(current()));
    error_at_current(parser, message, label, hint);
    xfree(label);
    xfree(message);

    return false;
}

/// Skip to the next decl
static void next_decl(Parser *parser)
{
    while (current() != TOKEN_IDENTIFIER && (peek(1) == TOKEN_COLON_COLON || peek(1) == TOKEN_COLON || peek(1) == TOKEN_COLON_EQ)) {
        advance();
    }
}

static void skip_newlines(Parser *parser)
{
    while (current() == TOKEN_NEWLINE) {
        advance();
    }
}

Index parse_expr(Parser *parser);
Index parse_type(Parser *parser);

// Parse an initialization:
//   - foo :: 5
//   - foo := 5
//   - foo : int : 5
//   - foo : int = 5
static Index parse_init(Parser *parser, Index identifier)
{
    char const *hint = "I can only comprehend these types of initialization:\n\n"
                       "    foo : int : 5\n"
                       "    bar := 5\n"
                       "    fizz: int = 6\n"
                       "    baz :: (integer: int) -> int {\n"
                       "        return integer\n"
                       "    }\n\n"
                       "Try writing one of these";

    if (match(TOKEN_COLON)) {
        Index init = reserve_node(parser);

        Index type = parse_type(parser);

        if (match(TOKEN_COLON)) {
            Index expr = parse_expr(parser);
            set_node(parser, init, NODE_CONST, identifier, (Data) {
                                                               .variable = { .type = type, .expr = expr },
                                                           });
        } else if (match(TOKEN_EQ)) {
            Index expr = parse_expr(parser);
            set_node(parser, init, NODE_VAR, identifier, (Data) {
                                                             .variable = { .type = type, .expr = expr },
                                                         });
        } else {
            char const *label = strf("found '%s'", token_to_string(current()));
            error_at_current(parser, "expected one of ':' or '='", label, hint);
            xfree(label);
            return invalid;
        }

        return init;
    } else if (match(TOKEN_COLON_EQ)) {
        Index init = reserve_node(parser);

        Index expr = parse_expr(parser);

        set_node(parser, init, NODE_VAR, identifier, (Data) {
                                                         .variable = { .expr = expr },
                                                     });
        return init;
    } else {
        if (!expect(TOKEN_COLON_COLON, hint))
            return invalid;

        Index init = reserve_node(parser);
        Index expr = parse_expr(parser);
        set_node(parser, init, NODE_CONST, identifier, (Data) {
                                                           .variable = { .expr = expr },
                                                       });
        return init;
    }
}

static Index parse_decl(Parser *parser)
{
    skip_newlines(parser);

    switch (current()) {
    case TOKEN_IDENTIFIER: {
        Index identifier = index();
        advance();

        // FIXME: handle generics

        Index decl = parse_init(parser, identifier);

        unused(decl);
    } break;
    case TOKEN_AT: {
        // FIXME: macros calls
    } break;
    case TOKEN_WHEN: {
        // FIXME: when directives
    } break;
    case TOKEN_FOREIGN: {
        // FIXME: foreign blocks
    } break;
    case TOKEN_BAD: {
        advance();
    } break;
    default: {
        // FIXME: Report an error
    } break;
    }
    return invalid;
}

static flatten void parse_decls(Parser *parser)
{
    while (current() != TOKEN_EOF) {
        Index decl = parse_decl(parser);
        if (decl == invalid) {
            next_decl(parser);
            decl = parse_decl(parser);
        }
        array_push(parser->decls, decl);
    }
}

Ast parse(FileId file_id, stringview src)
{
    LexedSrc lexed_src = lex(file_id, src);

    Parser parser = {
        .nodes = { 0 },
        .token_kind = lexed_src.kind,
        .token_start = lexed_src.start,
        .num_tokens = lexed_src.num_tokens,
        .diagnostics = lexed_src.diagnostics,
        .file_id = file_id,
    };

    size_t const size = parser.num_tokens / 3;
    array_reserve(parser.nodes.token, size);
    array_reserve(parser.nodes.kind, size);
    array_reserve(parser.nodes.data, size);

    add_node(&parser, NODE_ROOT, 0, (Data) { 0 });

    parse_decls(&parser);

    return (Ast) {
        .src = src,
        .token_kind = parser.token_kind,
        .token_start = parser.token_start,
        .nodes = parser.nodes,
        .decls = parser.decls,
        .diagnostics = parser.diagnostics
    };
}
