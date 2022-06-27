#include "parser.h"

/* TODO:
 * handle newlines correctly
 * handle comments
 * improve next_decl()
 * improve error handling
 * improve error reporting
 * parse import with parenthesis
 * parse_init() should handle a comma separated list of identifiers on lhs
 */

// Token macros
#define peek(_n) (parser->token_kind[parser->token_index + (_n)])
#define current() peek(0)
#define advance() (parser->token_index++)
#define index() (parser->token_index)

#define match(_kind) (__match(parser, (_kind)))
#define expect(_kind, _hint) (__expect(parser, (_kind), (_hint)))

#define extra(_data) (add_extra(parser->nodes.extra, (_data)))

#define foreach_scratch(_scratch_top, _ident, _body)              \
    for (size_t i = _scratch_top; i < parser->scratch_top; ++i) { \
        Node _ident = parser->scratch[i];                         \
        _body                                                     \
    }

#define push_scratch(_scratch_top)                              \
    do {                                                        \
        foreach_scratch(_scratch_top, elem, {                   \
            unused(array_pop(parser->scratch));                 \
            add_node(parser, elem.kind, elem.token, elem.data); \
        });                                                     \
        parser->scratch_top = _scratch_top;                     \
    } while (0)

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

    // Scratch
    array(Node) scratch;
    size_t scratch_top;
} Parser;

typedef enum ImportKind {
    IMPORT_NORMAL,
    IMPORT_FOREIGN,
} ImportKind;

enum { invalid = 0 };

static Index parse_type(Parser *parser);
static Index parse_expr(Parser *parser);
static Index parse_stmt(Parser *parser);
static void parse_decls(Parser *parser);

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

static inline void set_node(Parser *parser, Index index, NodeKind kind, Index token, Data data)
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

static inline void add_scratch(Parser *parser, Node scratch_node)
{
    array_push(parser->scratch, scratch_node);
    parser->scratch_top++;
}

static inline bool __match(Parser *parser, TokenKind kind)
{
    if (current() == kind) {
        advance();
        return true;
    }
    return false;
}

static inline void error_span(Parser *parser, Span span, char const *message, char const *label, char const *hint)
{
    Diagnostic diag = error(span, message, label, hint);
    array_push(parser->diagnostics, diag);
}

static flatten void error_at(Parser *parser, Index index, char const *message, char const *label, char const *hint)
{
    size_t start = parser->token_start[index];
    Token token = { .start = parser->str + start, .kind = parser->token_kind[index] };
    Span span = {
        .file_id = parser->file_id,
        .start = start,
        .end = start + token_length(token),
    };

    error_span(parser, span, message, label, hint);
}

static alwaysinline void error_at_current(Parser *parser, char const *message, char const *label, char const *hint)
{
    error_at(parser, index(), message, label, hint);
}

/// Create a Span, where:
///   - .start = start of the token at index 'first'
///   - .end   = end of the token at index 'last'
static Span span(Parser *parser, Index first, Index last)
{
    Token token = { .start = parser->str + parser->token_start[last], .kind = parser->token_kind[last] };
    return (Span) {
        .file_id = parser->file_id,
        .start = parser->token_start[first],
        .end = parser->token_start[last] + token_length(token) - 1, // NOTE: this -1 is temporary: search why this is needed
    };
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

// TODO: improve next_decl
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

static Index parse_type(Parser *parser)
{
    if (match(TOKEN_AND)) {
        if (match(TOKEN_MUT)) {
            return add_node(parser, NODE_REF_MUT_TYPE, index() - 2, (Data) {
                                                                        .unary = { .expr = parse_type(parser) },
                                                                    });
        } else if (match(TOKEN_OWN)) {
            return add_node(parser, NODE_REF_OWN_TYPE, index() - 2, (Data) {
                                                                        .unary = { .expr = parse_type(parser) },
                                                                    });
        } else {
            return add_node(parser, NODE_REF_TYPE, index() - 1, (Data) {
                                                                    .unary = { .expr = parse_type(parser) },
                                                                });
        }
    } else if (match(TOKEN_LBRACKET)) {
        Index lbracket = index() - 1;

        Index expr = parse_expr(parser);

        expect(TOKEN_RBRACKET, "If you were trying to describe an array type, you can write one like this:\n\n"
                               "    foo: [5]int\n\n"
                               "Try writing it like this");
        return add_node(parser, NODE_ARRAY_TYPE, lbracket, (Data) {
                                                               .binary = {
                                                                   .lhs = expr,
                                                                   .rhs = parse_type(parser),
                                                               },
                                                           });
    } else {
        return parse_expr(parser);
    }
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

    // NOTE: should we add an hint
    expect(TOKEN_RBRACE, NULL);

    Range range = { start, end };

    set_node(parser, result, NODE_BLOCK, token, (Data) { .block = range });

    return result;
}

#define STRUCT_EXAMPLE "\n\n    foo :: struct {\n" \
                       "        bar: int\n"        \
                       "        baz: float\n"      \
                       "    }\n"

static Index parse_struct(Parser *parser)
{
    Index token = index();
    assert(current() == TOKEN_STRUCT);
    advance();

    bool found
        = expect(TOKEN_LBRACE, "I was expecting an opening brace after the struct keyword. Try adding it!" STRUCT_EXAMPLE);
    if (!found)
        return invalid;

    size_t scratch_top = parser->scratch_top;
    Index result = reserve_node(parser);

    size_t count = 0;

    skip_newlines(parser);
    while (current() != TOKEN_EOF && current() != TOKEN_RBRACE) {
        // Field name
        Index name_token = index();
        expect(TOKEN_IDENTIFIER, "You can write a struct field like:" STRUCT_EXAMPLE "\n"
                                 "Probably you are trying to use a keyword as the field name");
        Index name = add_node(parser, NODE_IDENTIFIER, name_token, (Data) { 0 });

        // Field type
        expect(TOKEN_COLON, "Every field of a struct should have a type, like:" STRUCT_EXAMPLE "\n"
                            "Try adding the type to the field");
        Index type = parse_type(parser);

        Node field = {
            .kind = NODE_FIELD,
            .token = name,
            .data = {
                .binary = { .lhs = name, .rhs = type },
            },
        };

        add_scratch(parser, field);
        count++;

        if (current() == TOKEN_RBRACE || current() == TOKEN_EOF)
            break;

        expect(TOKEN_NEWLINE, "Every field of a struct must end with a newline. For example:" STRUCT_EXAMPLE "\n"
                              "Try adding a newline");
        skip_newlines(parser);
    }

    expect(TOKEN_RBRACE, "I was expecting a closing brace at the end of a struct declaration. Try adding it!" STRUCT_EXAMPLE);

    Index start = array_length(parser->nodes.kind);

    push_scratch(scratch_top);

    Index end = array_length(parser->nodes.kind) - 1;

    if (count == 0) {
        set_node(parser, result, NODE_STRUCT_TWO, token, (Data) {
                                                             .aggregate = { 0 },
                                                         });
    } else if (count <= 2) {
        set_node(parser, result, NODE_STRUCT_TWO, token, (Data) {
                                                             .aggregate = { start, end },
                                                         });
    } else {
        set_node(parser, result, NODE_STRUCT, token, (Data) {
                                                         .aggregate = { start, end },
                                                     });
    }

    return result;
}

#undef STRUCT_EXAMPLE

#define ENUM_EXAMPLE "\n\n    foo :: enum {\n" \
                     "        bar\n"           \
                     "        baz\n"           \
                     "    }\n"

static Node parse_enum_variant(Parser *parser)
{
    // Field name
    Index name_token = index();
    expect(TOKEN_IDENTIFIER, "You can write a struct field like:" ENUM_EXAMPLE "\n"
                             "Probably you are trying to use a keyword as the field name");
    Index name = add_node(parser, NODE_IDENTIFIER, name_token, (Data) { 0 });

    int count = -1;

    size_t scratch_top = parser->scratch_top;

    Index expr = invalid;

    if (match(TOKEN_LPAREN)) {
        count = 0;
        while (current() != TOKEN_EOF && current() != TOKEN_RPAREN) {
            Index lhs = parse_type(parser);

            Index rhs = invalid;
            if (match(TOKEN_COLON)) {
                if (parser->nodes.kind[lhs] != NODE_IDENTIFIER) {
                    char const *label = strf("found %s", token_to_string(parser->token_kind[lhs]));
                    error_at(parser, parser->nodes.token[lhs], "expected an identifier", label, "While I was trying to understand this enum variant I was expecting the left-hand side of the colon to be an identifier, while in the right-hand side of the colon I was expecting a type. For example:\n\n"
                                                                                                "    foo :: enum {\n"
                                                                                                "        bar(ident: string)\n"
                                                                                                "    }\n\n"
                                                                                                "Try making your variant look like this");
                    xfree(label);
                }
                rhs = parse_type(parser);
            }

            Node field = {
                .kind = NODE_FIELD,
                .token = lhs,
                .data = { .binary = { lhs, rhs } },
            };

            add_scratch(parser, field);
            count++;

            if (current() == TOKEN_RPAREN)
                break;

            // NOTE: is this needed?
            skip_newlines(parser);
            expect(TOKEN_COMMA, "I can only understand these types of enum variants:\n\n"
                                "    foo :: enum {\n"
                                "        first\n"
                                "        second = 2\n"
                                "        third(int)\n"
                                "        fourth(a: int, b: string)\n"
                                "    }\n");
            skip_newlines(parser);
        }

        expect(TOKEN_RPAREN, "I was expecting a closing parenthesis at the end of this complex variant, like:\n\n"
                             "    foo :: enum {\n"
                             "        bar(int, string)\n"
                             "    }\n");
    } else if (match(TOKEN_EQ)) {
        expr = parse_expr(parser);
    }

    if (count == -1) {
        return (Node) {
            .kind = NODE_VARIANT_SIMPLE,
            .token = name,
            .data = { .binary = { .lhs = expr } },
        };
    }

    if (count == 0) {
        Index rparen = index() - 1;
        error_span(parser, span(parser, name, rparen), "invalid enum variant", "there aren't any fields between the parenthesis", "An enum variant is invalid if it is written like:\n\n"
                                                                                                                                  "    foo :: enum {\n"
                                                                                                                                  "        bar()\n"
                                                                                                                                  "    }\n\n"
                                                                                                                                  "Instead of opening and closing the parenthesis, you can directly write an enum variant like:\n\n"
                                                                                                                                  "    foo :: enum {\n"
                                                                                                                                  "        bar\n"
                                                                                                                                  "    }\n\n");
    }

    Index start = array_length(parser->nodes.kind);

    push_scratch(scratch_top);

    Index end = array_length(parser->nodes.kind) - 1;

    NodeKind kind = count <= 2 ? NODE_VARIANT_TWO : NODE_VARIANT;
    return (Node) {
        .kind = kind,
        .token = name,
        .data = {
            .range = { start, end },
        }
    };
}

static Index parse_enum(Parser *parser)
{
    assert(current() == TOKEN_ENUM);
    advance();

    Index token = invalid;
    if (match(TOKEN_IDENTIFIER)) {
        token = parse_type(parser);
    }

    bool found = expect(TOKEN_LBRACE, "I was expecting an opening brace after the enum keyword. Try adding it!" ENUM_EXAMPLE);
    if (!found)
        return invalid;

    Index result = reserve_node(parser);

    size_t scratch_top = parser->scratch_top;

    int count = 0;

    while (current() != TOKEN_EOF && current() != TOKEN_RBRACE) {
        Node variant = parse_enum_variant(parser);

        add_scratch(parser, variant);
        count++;

        unused(match(TOKEN_COMMA));

        if (current() == TOKEN_RBRACE || current() == TOKEN_EOF)
            break;

        expect(TOKEN_NEWLINE, "Every variant of an enum must end with a newline. For example:" ENUM_EXAMPLE "\n"
                              "Try adding a newline");
        skip_newlines(parser);
    }

    expect(TOKEN_RBRACE, "I was expecting a closing brace at the end of an enum declaration. Try adding it!" ENUM_EXAMPLE);

    Index start = array_length(parser->nodes.kind);

    push_scratch(scratch_top);

    Index end = array_length(parser->nodes.kind) - 1;

    if (count == 0) {
        set_node(parser, result, NODE_ENUM_TWO, token, (Data) {
                                                           .aggregate = { 0 },
                                                       });
    } else if (count <= 2) {
        set_node(parser, result, NODE_ENUM_TWO, token, (Data) {
                                                           .aggregate = { start, end },
                                                       });
    } else {
        set_node(parser, result, NODE_ENUM, token, (Data) {
                                                       .aggregate = { start, end },
                                                   });
    }

    return result;
}

#undef ENUM_EXAMPLE

static int parse_function_params(Parser *parser, Range *range)
{
    assert(current() == TOKEN_LPAREN);
    advance();

    if (match(TOKEN_RPAREN)) {
        return 0;
    }

    /* array(Node) params = NULL; */
    size_t count = 0;
    size_t scratch_top = parser->scratch_top;

    bool vararg = false;

    while (current() == TOKEN_IDENTIFIER) {
        Index name_token = index();
        advance();
        Index name = add_node(parser, NODE_IDENTIFIER, name_token, (Data) { 0 });

        if (match(TOKEN_COLON)) {
            if (match(TOKEN_ELLIPSIS)) {
                vararg = true;
            }
            Index type = parse_type(parser);

            Index expr = invalid;
            if (match(TOKEN_EQ)) {
                expr = parse_expr(parser);
            }

            Node param = {
                .kind = vararg ? NODE_VARPARAM : NODE_PARAM,
                .token = name,
                .data = {
                    .param = { type, expr },
                },
            };
            vararg = false;

            /* array_push(params, param); */
            add_scratch(parser, param);
            count++;
        } else {
            if (current() != TOKEN_COMMA)
                return -1;

            char const *start = parser->str + parser->token_start[parser->token_index - 1];
            TokenKind kind = parser->token_kind[parser->token_index - 1];
            Token token = { .start = start, .kind = kind };
            stringview param = { .str = start, .length = token_length(token) };

            char const *label = strf("found %s", token_to_string(current()));
            char const *hint = strf("I was expecting a parameter with a type, like:\n\n"
                                    "    foo :: (bar: int)\n"
                                    "Try adding a type to the parameter " SV_FMT,
                SV_ARGS(param));
            error_at_current(parser, "expected a type", label, hint);
            xfree(hint);
            xfree(label);
        }

        // NOTE: should we actually exit right here when we encounter TOKEN_EOF
        if (current() == TOKEN_RPAREN || current() == TOKEN_EOF)
            break;

        expect(TOKEN_COMMA, "I was expecting a comma after the parameter, like:\n\n"
                            "    foo :: (bar: int, baz: int) -> int\n\n"
                            "Try adding it");
        skip_newlines(parser);
    }

    expect(TOKEN_RPAREN, "I was expecting a closing parenthesis after the function parameters. Try adding it!");

    Index start = array_length(parser->nodes.kind);

    push_scratch(scratch_top);

    Index end = array_length(parser->nodes.kind) - 1;

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

    Index return_type = invalid;
    if (match(TOKEN_ARROW)) {
        return_type = parse_type(parser);
    }

    Index calling_convention = invalid;
    if (match(TOKEN_STRING)) {
        calling_convention = add_node(parser, NODE_STRING, index() - 1, (Data) { 0 });
    }

    skip_newlines(parser);
    Index body;
    if (match(TOKEN_FAT_ARROW)) {
        body = parse_expr(parser);
    } else {
        body = parse_block(parser);
    }

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
    case TOKEN_IDENTIFIER: {
        advance();
        return add_node(parser, NODE_IDENTIFIER, index() - 1, (Data) { 0 });
    } break;
    case TOKEN_INT: {
        advance();
        return add_node(parser, NODE_INT, index() - 1, (Data) { 0 });
    }
    case TOKEN_LPAREN: {
        Index expr = parse_function(parser);
        if (expr != invalid)
            return expr;

        advance();
        expr = parse_expr(parser);
        expect(TOKEN_RPAREN, "I was expecting a closing parenthesis. Try adding it!");

        return expr;
    } break;
    case TOKEN_STRUCT: {
        return parse_struct(parser);
    } break;
    case TOKEN_ENUM: {
        return parse_enum(parser);
    } break;
    case TOKEN_RBRACKET: {
        // NOTE: This is needed to parse array types in parse_type()
        return invalid;
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

static Range parse_name_list(Parser *parser)
{
    Index start = array_length(parser->nodes.kind);

    while (current() != TOKEN_EOF && match(TOKEN_IDENTIFIER)) {
        add_node(parser, NODE_IDENTIFIER, index() - 1, (Data) { 0 });
        if (current() != TOKEN_COMMA)
            break;
        advance();
    }

    Index end = array_length(parser->nodes.kind) - 1;
    if (end < start) {
        return (Range) { 0 };
    }
    return (Range) { start, end };
}

// Parse an initialization:
//   - foo :: 5
//   - foo := 5
//   - foo : int : 5
//   - foo : int = 5
static Index parse_init(Parser *parser, Index identifier)
{
    char const *hint = "I can only understand these types of initialization:\n\n"
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

static Index parse_import(Parser *parser, ImportKind import_kind)
{
    assert(current() == TOKEN_IMPORT);
    advance();

    Index result = reserve_node(parser);

    Index module = index();
    advance();

    Index list = invalid;
    if (match(TOKEN_LBRACE)) {
        list = reserve_node(parser);

        Range list_range = parse_name_list(parser);

        if (list_range.start != 0 && list_range.end != 0) {
            set_node(parser, list, NODE_RANGE, invalid, (Data) { .range = list_range });
        } else {
            if (match(TOKEN_ELLIPSIS)) {
                set_node(parser, list, NODE_ALL_SYMBOLS, index() - 1, (Data) { 0 });
            } else {
                char const *label = strf("found %s", token_to_string(current()));
                error_at_current(parser, "expected either an identifier or ...", label, "I can only understand these two type of complex imports:\n\n"
                                                                                        "    import foo { bar, baz }\n"
                                                                                        "    import foo { ... }\n\n"
                                                                                        "Try making your import look like this");
                xfree(label);
            }
        }

        expect(TOKEN_RBRACE, "At the end of a symbol list it's required a closing brace, like:\n\n"
                             "    import foo { bar, baz }\n\n"
                             "Try adding it");
    }

    Index as_name = invalid;
    if (match(TOKEN_AS)) {
        as_name = index();
        advance();
    }

    if (list != invalid) {
        NodeKind kind = import_kind == IMPORT_FOREIGN ? NODE_FOREIGN_IMPORT_COMPLEX : NODE_IMPORT_COMPLEX;
        set_node(parser, result, kind, module, (Data) {
                                                   .binary = { .lhs = as_name, .rhs = list },
                                               });
    } else {
        NodeKind kind = import_kind == IMPORT_FOREIGN ? NODE_FOREIGN_IMPORT : NODE_IMPORT;
        set_node(parser, result, kind, module, (Data) {
                                                   .binary = { .lhs = as_name },
                                               });
    }

    return result;
}

static Index parse_decl(Parser *parser)
{
    skip_newlines(parser);

    switch (current()) {
    case TOKEN_IDENTIFIER: {
        Index identifier = add_node(parser, NODE_IDENTIFIER, index() - 1, (Data) { 0 });
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
    case TOKEN_USING: {
        // FIXME: using
    } break;
    case TOKEN_IMPORT: {
        return parse_import(parser, IMPORT_NORMAL);
    } break;
    case TOKEN_FOREIGN: {
        Index foreign = index();
        advance();

        if (current() == TOKEN_IMPORT)
            return parse_import(parser, IMPORT_FOREIGN);

        bool found = expect(TOKEN_LBRACE, "I expected a foreign declaration, like:\n\n"
                                          "    foreign {\n"
                                          "        bar :: () -> rawptr\n"
                                          "    }\n\n"
                                          "Or a foreign import, like:\n\n"
                                          "    foreign import sdl\n\n"
                                          "Try making your foreign declaration look like one of these two");
        if (!found)
            return invalid;

        Index result = reserve_node(parser);

        if (match(TOKEN_RBRACE)) {
            set_node(parser, result, NODE_FOREIGN, foreign, (Data) {
                                                                .block = { 0 },
                                                            });
            return result;
        }

        Index start = array_length(parser->nodes.kind);

        parse_decls(parser);

        Index end = array_length(parser->nodes.kind) - 1;

        expect(TOKEN_RBRACE, "I was expecting a closing brace at the end of a foreign block. Try adding it!");

        set_node(parser, result, NODE_FOREIGN, foreign, (Data) {
                                                            .block = { start, end },
                                                        });
        return result;
    } break;
    case TOKEN_BAD: {
        advance();
    } break;
    default: {
        error_at_current(parser, "invalid declaration", "", "Try writing some declarations like:\n\n"
                                                            "    import os\n\n"
                                                            "    foo :: struct {\n"
                                                            "        bar: int\n"
                                                            "        baz: float\n"
                                                            "    }\n\n"
                                                            "    main :: () {\n"
                                                            "        value := foo(bar: 1, baz: 1.0)\n"
                                                            "        println(\"value is {}\", value)\n"
                                                            "    }\n");
    } break;
    }
    return invalid;
}

static flatten void parse_decls(Parser *parser)
{
    skip_newlines(parser);
    while (current() != TOKEN_EOF) {
        Index decl = parse_decl(parser);
        if (decl == invalid) {
            next_decl(parser);
            decl = parse_decl(parser);
        }
        array_push(parser->decls, decl);
        skip_newlines(parser);
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
