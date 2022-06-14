#ifndef PARSER_H_
#define PARSER_H_

#include "ast.h"
#include "util.h"
#include "vfs.h"

Ast parse(FileId file_id, stringview src);

#endif // PARSER_H_
