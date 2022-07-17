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

#include "printer.h"
#include "ast.h"
#include "lexer.h"

#define kind() printer->ast.nodes.kind[printer->index]
#define data() printer->ast.nodes.data[printer->index]
#define token() printer->ast.nodes.token[printer->index]
#define set(_val) printer->index = (_val)

/// Internal data structure used to generate the s-expression
typedef struct Printer {
    Ast ast;
    size_t index;
} Printer;

/// Generate the string of a token at 'index'
static stringview token_at(Printer *printer, size_t token_index, TokenKind kind)
{
    size_t index = printer->ast.token_start[token_index];
    char const *str = printer->ast.src.str + index;
    size_t length = token_length((Token) {
        .kind = kind,
        .start = str,
    });
    return stringview_make(str, length);
}

static stringview print_expr(Printer *printer);

static stringview print_binary(Printer *printer, char const *op)
{
    set(data().binary.lhs);
    stringview lhs = print_expr(printer);

    set(data().binary.rhs);
    stringview rhs = print_expr(printer);

    char const *str = strf("(%s " SV_FMT " " SV_FMT ")", op, SV_ARGS(lhs), SV_ARGS(rhs));
    return stringview_from_cstr(str);
}

#define BINARY(_kind, _op)                  \
    case (_kind): {                         \
        return print_binary(printer, #_op); \
    } break

/* clang-format off */
static stringview print_expr(Printer *printer)
{
    switch (kind()) {
    case NODE_IDENTIFIER: {
        return token_at(printer, token(), TOKEN_IDENTIFIER);
    } break;
    case NODE_INT: {
        return token_at(printer, token(), TOKEN_INT);
    } break;
    BINARY(NODE_PIPE_EXPR, | >);
    BINARY(NODE_OR, or);
    BINARY(NODE_OR_EXPR, ||);
    BINARY(NODE_AND_EXPR, &&);
    BINARY(NODE_EQ_EXPR, ==);
    BINARY(NODE_NOTEQ_EXPR, !=);
    BINARY(NODE_LT_EXPR, <);
    BINARY(NODE_GT_EXPR, >);
    BINARY(NODE_LTEQ_EXPR, <=);
    BINARY(NODE_GTEQ_EXPR, >=);
    BINARY(NODE_ADD_EXPR, +);
    BINARY(NODE_SUB_EXPR, -);
    BINARY(NODE_BITXOR_EXPR, ^);
    BINARY(NODE_BITOR_EXPR, |);
    BINARY(NODE_MUL_EXPR, *);
    BINARY(NODE_DIV_EXPR, /);
    BINARY(NODE_MOD_EXPR, %);
    BINARY(NODE_BITAND_EXPR, &);
    BINARY(NODE_LSHIFT_EXPR, <<);
    BINARY(NODE_RSHIFT_EXPR, >>);
    BINARY(NODE_AS_EXPR, as);
    default: {
        return stringview_from_cstr("");
    } break;
    }
}
/* clang-format on */

#undef BINARY

static stringview print_decl(Printer *printer)
{
    switch (kind()) {
    case NODE_CONST: {
        stringview ident = token_at(printer, token(), TOKEN_IDENTIFIER);

        // Generate the string for the expression
        set(data().variable.expr);
        stringview expr = print_expr(printer);

        char const *str = strf("(def " SV_FMT " " SV_FMT ")\n", SV_ARGS(ident), SV_ARGS(expr));
        return stringview_from_cstr(str);
    } break;
    default: {
        return stringview_from_cstr("");
    } break;
    }
}

stringview print_ast(Ast ast)
{
    size_t decl_length = array_length(ast.decls);

    Printer printer = { .ast = ast };

    char const *buf = "";

    for (size_t i = 0; i < decl_length; ++i) {
        printer.index = i;
        buf = strf("%s" SV_FMT, buf, SV_ARGS(print_decl(&printer)));
    }

    return stringview_from_cstr(buf);
}
