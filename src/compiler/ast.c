#include "ast.h"

void free_ast(Ast ast)
{
    array_free(ast.token_kind);
    array_free(ast.token_start);
    array_free(ast.nodes.kind);
    array_free(ast.nodes.data);
    array_free(ast.nodes.token);
    array_free(ast.nodes.extra);
    array_free(ast.decls);
    array_free(ast.diagnostics);
}
