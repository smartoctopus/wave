#include "parser.h"

// Token macros
#define peek(_n) (parser->token_kind[parser->token_index + (_n)])
#define current() peek(0)
#define advance() (parser->token_index++)
#define index() (parser->token_index)

#define match(_kind) (__match(parser, (_kind)))
#define expect(_kind, _hint) (__expect(parser, (_kind), (_hint)))

#define extra(_data) (add_extra(parser->nodes.extra, (_data)))

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

static void pop_node(Parser *parser, Index index)
{
    if (index != array_length(parser->nodes.kind) - 1) {
        panic("Internal Compiler Error: popping a node that isn't at the end of the array");
    }

    unused(array_pop(parser->nodes.kind));
    unused(array_pop(parser->nodes.token));
    unused(array_pop(parser->nodes.data));
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

static Index parse_type(Parser *parser);
static Index parse_expr(Parser *parser);
static Index parse_stmt(Parser *parser);

static Index parse_type(Parser *parser)
{
    unused(parser);
    return invalid;
}

static Index parse_stmt(Parser *parser)
{
    unused(parser);
    return invalid;
}

static Index parse_block(Parser *parser)
{
    if (current() != TOKEN_LBRACE) {
        return invalid;
    }

    Index token = index();
    advance();

    Index result = reserve_node(parser);

    Index start = 0;
    Index end = 0;

    while (current() != TOKEN_EOF && current() != TOKEN_RBRACE) {
        skip_newlines(parser);
        Index stmt = parse_stmt(parser);
        if (unlikely(start == 0)) {
            start = stmt;
        } else {
            end = stmt;
        }
    }

    advance();

    Range range = { start, end };

    set_node(parser, result, NODE_BLOCK, token, (Data) { .block = range });

    return result;
}

static Index parse_struct(Parser *parser)
{
    assert(current() == TOKEN_STRUCT);
    advance();
    return invalid;
}

// FIXME: design errors for *ALL* function parsing
static int parse_function_params(Parser *parser, Range *range)
{
    assert(current() == TOKEN_LPAREN);
    advance();

    if (match(TOKEN_RPAREN)) {
        return 0;
    }

    array(Node) params = NULL;
    size_t count = 0;

    while (current() == TOKEN_IDENTIFIER) {
        Index ident = index();
        advance();

        if (match(TOKEN_COLON)) {
            // FIXME: check for variadic parameters
            Index type = parse_type(parser);

            Index expr = invalid;
            if (match(TOKEN_EQ)) {
                expr = parse_expr(parser);
            }

            Node param = {
                .kind = NODE_PARAM,
                .token = ident,
                .data = {
                    .param = { type, expr },
                },
            };

            array_push(params, param);
            count++;
        } else {
            if (current() != TOKEN_COMMA)
                return -1;

            // FIXME: report missing type
        }

        if (match(TOKEN_RPAREN))
            break;

        expect(TOKEN_COMMA, "");
    }

    Index start = array_length(parser->nodes.kind);

    for (size_t i = 0; i < count; ++i) {
        Node param = params[i];
        add_node(parser, param.kind, param.token, param.data);
    }

    Index end = array_length(parser->nodes.kind) - 1;

    array_free(params);

    *range = (Range) { start, end };
    return count;
}

static Index parse_function(Parser *parser)
{
    Index result = reserve_node(parser);
    Index func_proto = reserve_node(parser);

    Index start = index();
    Range args;
    int count = parse_function_params(parser, &args);

    if (count == -1) {
        // Pop the function return value
        pop_node(parser, func_proto);
        pop_node(parser, result);

        // Reset the cursor
        parser->token_index = start;
        return invalid;
    }

    expect(TOKEN_RPAREN, "");

    Index return_type = invalid;
    if (match(TOKEN_ARROW)) {
        return_type = parse_type(parser);
    }

    Index calling_convention = invalid;
    if (match(TOKEN_STRING)) {
        calling_convention = index() - 1;
    }

    Index body = parse_block(parser);

    // Set func-proto
    if (count == 0) {
        FuncProtoOne data = { invalid, calling_convention };
        Index proto = extra(data);
        set_node(parser, func_proto, NODE_FUNC_PROTO_ONE, invalid, (Data) {
                                                                       .func_proto = { proto, return_type },
                                                                   });
    } else if (count == 1) {
        FuncProtoOne data = { args.start, calling_convention };
        Index proto = extra(data);
        set_node(parser, func_proto, NODE_FUNC_PROTO_ONE, invalid, (Data) {
                                                                       .func_proto = { proto, return_type },
                                                                   });
    } else {
        FuncProto data = { args, calling_convention };
        Index proto = extra(data);
        set_node(parser, func_proto, NODE_FUNC_PROTO, invalid, (Data) {
                                                                   .func_proto = { proto, return_type },
                                                               });
    }
    set_node(parser, result, NODE_FUNC, start, (Data) {
                                                   .func = { func_proto, body },
                                               });

    return result;
}

static Index parse_operand(Parser *parser)
{
    switch (current()) {
    case TOKEN_LPAREN: {
        Index expr = parse_function(parser);
        if (expr != invalid)
            return expr;

        advance();
        expr = parse_expr(parser);
        expect(TOKEN_RPAREN, "");

        return expr;
    } break;
    case TOKEN_STRUCT: {
        return parse_struct(parser);
    } break;
    default: {
        todo();
    } break;
    }
    return invalid;
}

static Index parse_expr(Parser *parser)
{
    Index lhs = parse_operand(parser);
    return lhs;
}

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

        return decl;
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
