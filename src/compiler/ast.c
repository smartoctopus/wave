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
    free_diagnostics(&ast.diagnostics);
}

uint32_t __add_extra(char **buf, void *data, size_t len)
{
    size_t result = array_length(*buf);
    array_maybegrow(*buf, len);
    memcpy(*buf + result, data, len);
    return result;
}
