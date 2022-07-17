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

#include "ast.h"
#include "vfs.h"
#define UTIL_IMPL
#include "util.h"

#include "diagnostic.h"
#include "parser.h"
#include "test.h"

test("Wave compiler")
{
    describe("Lexer")
    {
        static LexedSrc lexed_src = { 0 };
        after_each()
        {
            if (lexed_src.kind != NULL) {
                expect(array_last(lexed_src.kind) == TOKEN_EOF);
            }
            array_free(lexed_src.kind);
            array_free(lexed_src.start);
            lexed_src = (LexedSrc) { 0 };
        }

        it("should lex an empty string")
        {
            lexed_src = lex(0, stringview_from_cstr(""));
        }

        it("should skip unknown characters")
        {
            lexed_src = lex(0, stringview_from_cstr("$"));
            expect(lexed_src.kind[0] == TOKEN_BAD);
        }

        it("should skip spaces and tabs")
        {
            lexed_src = lex(0, stringview_from_cstr("    \t\t\t"));
        }

        it("should lex operators")
        {
            lexed_src = lex(0, stringview_from_cstr("+ - * / % & | ^ && || |> < > <= >= == != << >> ! ? => -> ~ , ; : :: . .. ... @ (  ) [  ] { } += -= *= /= %= &= |= ^= <<= >>= :="));
            TokenKind kinds[] = {
                TOKEN_PLUS,
                TOKEN_MINUS,
                TOKEN_STAR,
                TOKEN_SLASH,
                TOKEN_PERCENTAGE,
                TOKEN_AND,
                TOKEN_PIPE,
                TOKEN_CARET,
                TOKEN_AND_AND,
                TOKEN_PIPE_PIPE,
                TOKEN_PIPE_GT,
                TOKEN_LT,
                TOKEN_GT,
                TOKEN_LT_EQ,
                TOKEN_GT_EQ,
                TOKEN_EQ_EQ,
                TOKEN_EXCLAMATION_EQ,
                TOKEN_LT_LT,
                TOKEN_GT_GT,
                TOKEN_EXCLAMATION,
                TOKEN_QUESTION,
                TOKEN_FAT_ARROW,
                TOKEN_ARROW,
                TOKEN_TILDE,
                TOKEN_COMMA,
                TOKEN_SEMICOLON,
                TOKEN_COLON,
                TOKEN_COLON_COLON,
                TOKEN_DOT,
                TOKEN_DOT_DOT,
                TOKEN_ELLIPSIS,
                TOKEN_AT,
                TOKEN_LPAREN,
                TOKEN_RPAREN,
                TOKEN_LBRACKET,
                TOKEN_RBRACKET,
                TOKEN_LBRACE,
                TOKEN_RBRACE,
                TOKEN_PLUS_EQ,
                TOKEN_MINUS_EQ,
                TOKEN_STAR_EQ,
                TOKEN_SLASH_EQ,
                TOKEN_PERCENTAGE_EQ,
                TOKEN_AND_EQ,
                TOKEN_PIPE_EQ,
                TOKEN_CARET_EQ,
                TOKEN_LT_LT_EQ,
                TOKEN_GT_GT_EQ,
                TOKEN_COLON_EQ,
            };
            for (size_t i = 0; i < lengthof(kinds); ++i) {
                char const *str1 = token_to_string(lexed_src.kind[i]);
                char const *str2 = token_to_string(kinds[i]);
                char const *str = strf("%d) %s = %s", i, str1, str2);
                expect_str(lexed_src.kind[i] == kinds[i], str);
                xfree(str);
            }
        }

        it("should lex integers")
        {
            lexed_src = lex(0, stringview_from_cstr("1_234  0b110  0o01234_567  0x0123456789_ABCDEF"));
            expect(lexed_src.kind[0] == TOKEN_INT);
            expect(lexed_src.kind[1] == TOKEN_INT);
            expect(lexed_src.kind[2] == TOKEN_INT);
            expect(lexed_src.kind[3] == TOKEN_INT);
        }

        it("should lex floating point numbers")
        {
            lexed_src = lex(0, stringview_from_cstr("1.2  1e+2 0x1_p2  0x1.2p-2"));
            expect(lexed_src.kind[0] == TOKEN_FLOAT);
            expect(lexed_src.kind[1] == TOKEN_FLOAT);
            expect(lexed_src.kind[2] == TOKEN_FLOAT);
            expect(lexed_src.kind[3] == TOKEN_FLOAT);
        }

        it("should lex a char")
        {
            lexed_src = lex(0, stringview_from_cstr("'c' '\xFF' '\t'"));
            expect(lexed_src.kind[0] == TOKEN_CHAR);
            expect(lexed_src.kind[1] == TOKEN_CHAR);
            expect(lexed_src.kind[2] == TOKEN_CHAR);
        }

        it("should lex a string")
        {
            lexed_src = lex(0, stringview_from_cstr("\"Hello, World\" \"\"\" Multiline string \"\"\""));
            expect(lexed_src.kind[0] == TOKEN_STRING);
            expect(lexed_src.kind[1] == TOKEN_MULTILINE_STRING);
        }

        it("should lex identifiers")
        {
            lexed_src = lex(0, stringview_from_cstr("hello1234  __world  va_123lue  function"));
            expect(lexed_src.kind[0] == TOKEN_IDENTIFIER);
            expect(lexed_src.kind[1] == TOKEN_IDENTIFIER);
            expect(lexed_src.kind[2] == TOKEN_IDENTIFIER);
            expect(lexed_src.kind[3] == TOKEN_IDENTIFIER);
        }

        it("should lex keywords")
        {
            lexed_src = lex(0, stringview_from_cstr("as  alignof  asm  break  continue  context  defer  distinct  else  enum  for  foreign  fallthrough  if  in  import  mut  match  map  new  own  or  offsetof  return  struct  sizeof  typeof  using  union  undef  where  when"));
            expect(lexed_src.kind[0] == TOKEN_AS);
            expect(lexed_src.kind[1] == TOKEN_ALIGNOF);
            expect(lexed_src.kind[2] == TOKEN_ASM);
            expect(lexed_src.kind[3] == TOKEN_BREAK);
            expect(lexed_src.kind[4] == TOKEN_CONTINUE);
            expect(lexed_src.kind[5] == TOKEN_CONTEXT);
            expect(lexed_src.kind[6] == TOKEN_DEFER);
            expect(lexed_src.kind[7] == TOKEN_DISTINCT);
            expect(lexed_src.kind[8] == TOKEN_ELSE);
            expect(lexed_src.kind[9] == TOKEN_ENUM);
            expect(lexed_src.kind[10] == TOKEN_FOR);
            expect(lexed_src.kind[11] == TOKEN_FOREIGN);
            expect(lexed_src.kind[12] == TOKEN_FALLTHROUGH);
            expect(lexed_src.kind[13] == TOKEN_IF);
            expect(lexed_src.kind[14] == TOKEN_IN);
            expect(lexed_src.kind[15] == TOKEN_IMPORT);
            expect(lexed_src.kind[16] == TOKEN_MUT);
            expect(lexed_src.kind[17] == TOKEN_MATCH);
            expect(lexed_src.kind[18] == TOKEN_MAP);
            expect(lexed_src.kind[19] == TOKEN_NEW);
            expect(lexed_src.kind[20] == TOKEN_OWN);
            expect(lexed_src.kind[21] == TOKEN_OR);
            expect(lexed_src.kind[22] == TOKEN_OFFSETOF);
            expect(lexed_src.kind[23] == TOKEN_RETURN);
            expect(lexed_src.kind[24] == TOKEN_STRUCT);
            expect(lexed_src.kind[25] == TOKEN_SIZEOF);
            expect(lexed_src.kind[26] == TOKEN_TYPEOF);
            expect(lexed_src.kind[27] == TOKEN_USING);
            expect(lexed_src.kind[28] == TOKEN_UNION);
            expect(lexed_src.kind[29] == TOKEN_UNDEF);
            expect(lexed_src.kind[30] == TOKEN_WHERE);
            expect(lexed_src.kind[31] == TOKEN_WHEN);
        }

        it("should lex newlines")
        {
            lexed_src = lex(0, stringview_from_cstr("\n\n"));
            expect(lexed_src.kind[0] == TOKEN_NEWLINE);
            expect(lexed_src.kind[1] == TOKEN_NEWLINE);
        }

        it("should lex and skip comments")
        {
            lexed_src = lex(0, stringview_from_cstr("// This is a comment\n /* This is /* also */ a comment */\n /// This is a doc comment "));
            expect(lexed_src.kind[0] == TOKEN_COMMENT);
            expect(lexed_src.kind[1] == TOKEN_NEWLINE);
            expect(lexed_src.kind[2] == TOKEN_MULTILINE_COMMENT);
            expect(lexed_src.kind[3] == TOKEN_NEWLINE);
            expect(lexed_src.kind[4] == TOKEN_DOC_COMMENT);
            expect(lexed_src.kind[5] == TOKEN_NEWLINE);
        }
    }

    describe("Parser")
    {
        static Ast ast = { 0 };
        after_each()
        {
            if (array_length(ast.diagnostics) > 0) {
                emit_diagnostics(ast.diagnostics);
            }
            free_ast(ast);
            ast = (Ast) { 0 };
            vfs_cleanup();
        }

        it("should parse an empty string")
        {
            ast = parse(0, stringview_from_cstr(""));
            expect(ast.nodes.kind[0] == NODE_ROOT);
            expect(array_length(ast.nodes.kind) == 1);
        }

        it("should parse a function without parameters")
        {
            stringview content = stringview_from_cstr("main :: () {\n}");
            FileId id = add_file("test.wave", content);
            ast = parse(id, content);
            expect(ast.nodes.kind[2] == NODE_CONST);
            Index func = ast.nodes.data[2].variable.expr;
            expect(ast.nodes.kind[func] == NODE_FUNC);
            // Function prototype
            Index index = ast.nodes.data[func].func.func_proto;
            expect(ast.nodes.kind[index] == NODE_FUNC_PROTO_ONE);
            Index proto_index = ast.nodes.data[index].func_proto.proto;
            FuncProtoOne func_proto = get_extra(ast.nodes.extra, FuncProtoOne, proto_index);
            expect(func_proto.param == 0);
            expect(func_proto.calling_convention == 0);
            Index return_type = ast.nodes.data[index].func_proto.return_type;
            expect(return_type == 0);
            // Function body
            Index body = ast.nodes.data[func].func.body;
            expect(ast.nodes.kind[body] == NODE_BLOCK);
            expect(ast.nodes.data[body].block.start == 0);
            expect(ast.nodes.data[body].block.end == 0);
        }

        it("should parse a function with one parameter")
        {
            stringview content = stringview_from_cstr("main :: (args: []string) {\n}\n");
            FileId id = add_file("test.wave", content);
            ast = parse(id, content);
            Index func = ast.nodes.data[2].variable.expr;
            Index index = ast.nodes.data[func].func.func_proto;
            Index proto_index = ast.nodes.data[index].func_proto.proto;
            FuncProtoOne func_proto = get_extra(ast.nodes.extra, FuncProtoOne, proto_index);
            Index param = func_proto.param;
            expect(ast.nodes.kind[param] == NODE_PARAM);
            Index type = ast.nodes.data[param].param.type;
            expect(ast.nodes.kind[type] == NODE_ARRAY_TYPE);
            expect(ast.nodes.data[type].binary.lhs == 0);
            Index array_type = ast.nodes.data[type].binary.rhs;
            expect(ast.nodes.kind[array_type] == NODE_IDENTIFIER);
            expect(ast.nodes.data[param].param.expr == 0);
        }

        it("should parse a function with multple parameters")
        {
            stringview content = stringview_from_cstr("main :: (arg1: int, arg2: int, arg3: int, arg4: int, arg5: int, arg6: int) {\n}\n");
            FileId id = add_file("test.wave", content);
            ast = parse(id, content);
            Index func = ast.nodes.data[2].variable.expr;
            Index index = ast.nodes.data[func].func.func_proto;
            Index proto_index = ast.nodes.data[index].func_proto.proto;
            FuncProto func_proto = get_extra(ast.nodes.extra, FuncProto, proto_index);
            Range params = func_proto.params;
            int count = 1;
            for (size_t i = params.start; i <= params.end; ++i) {
                char const *kind_str = strf("%d) kind", count);
                char const *type_str = strf("%d) type", count);
                char const *expr_str = strf("%d) expr", count);
                expect_str(ast.nodes.kind[i] == NODE_PARAM, kind_str);
                Index type = ast.nodes.data[i].param.type;
                expect_str(ast.nodes.kind[type] == NODE_IDENTIFIER, type_str);
                expect_str(ast.nodes.data[i].param.expr == 0, expr_str);
                xfree(kind_str);
                xfree(type_str);
                xfree(expr_str);

                count++;
            }
        }

        it("should parse a structure without fields")
        {
            stringview content = stringview_from_cstr("foo :: struct {}\n");
            FileId id = add_file("test.wave", content);
            ast = parse(id, content);
            Index aggregate = ast.nodes.data[2].variable.expr;
            expect(ast.nodes.kind[aggregate] == NODE_STRUCT_TWO);
            Index start = ast.nodes.data[aggregate].aggregate.start;
            Index end = ast.nodes.data[aggregate].aggregate.end;
            expect(end == 0);
            expect(start == 0);
        }

        it("should parse a structure with one field")
        {
            stringview content = stringview_from_cstr("foo :: struct {bar: int\n}\n");
            FileId id = add_file("test.wave", content);
            ast = parse(id, content);
            Index aggregate = ast.nodes.data[2].variable.expr;
            expect(ast.nodes.kind[aggregate] == NODE_STRUCT_TWO);
            Index start = ast.nodes.data[aggregate].aggregate.start;
            Index end = ast.nodes.data[aggregate].aggregate.end;
            expect(end - start == 0);
            expect(ast.nodes.kind[start] == NODE_FIELD);
            Index type = ast.nodes.data[start].binary.lhs;
            expect(ast.nodes.kind[type] == NODE_IDENTIFIER);
        }

        it("should parse a structure with two field")
        {
            stringview content = stringview_from_cstr("foo :: struct {bar: int,\n baz: [5]int\n}\n");
            FileId id = add_file("test.wave", content);
            ast = parse(id, content);
            Index aggregate = ast.nodes.data[2].variable.expr;
            expect(ast.nodes.kind[aggregate] == NODE_STRUCT_TWO);
            Index start = ast.nodes.data[aggregate].aggregate.start;
            Index end = ast.nodes.data[aggregate].aggregate.end;
            expect(end - start == 1);
            expect(ast.nodes.kind[start] == NODE_FIELD);
            expect(ast.nodes.kind[end] == NODE_FIELD);
            Index type = ast.nodes.data[start].binary.lhs;
            expect(ast.nodes.kind[type] == NODE_IDENTIFIER);

            type = ast.nodes.data[end].binary.lhs;
            expect(ast.nodes.kind[type] == NODE_ARRAY_TYPE);
            Index array_type = ast.nodes.data[type].binary.rhs;
            expect(ast.nodes.kind[array_type] == NODE_IDENTIFIER);
            Index array_expr = ast.nodes.data[type].binary.lhs;
            expect(ast.nodes.kind[array_expr] == NODE_INT);
        }

        it("should parse a structure with multiple fields")
        {
            // FIXME: when we can parse the types check them
            stringview content = stringview_from_cstr("foo :: struct {a: int,\n b: string,\nc: char\n}");
            FileId id = add_file("test.wave", content);
            ast = parse(id, content);
            Index aggregate = ast.nodes.data[2].variable.expr;
            expect(ast.nodes.kind[aggregate] == NODE_STRUCT);
            Index start = ast.nodes.data[aggregate].aggregate.start;
            Index end = ast.nodes.data[aggregate].aggregate.end;
            int count = 1;
            for (Index i = start; i <= end; ++i) {
                char const *str = strf("%d field", count);
                char const *type_str = strf("%d field's type", count);
                expect_str(ast.nodes.kind[i] == NODE_FIELD, str);
                Index type = ast.nodes.data[i].binary.lhs;
                expect_str(ast.nodes.kind[type] == NODE_IDENTIFIER, type_str);
                xfree(type_str);
                xfree(str);
                count++;
            }
        }

        it("should parse an enum without variants")
        {
            stringview content = stringview_from_cstr("foo :: enum {}");
            FileId id = add_file("test.wave", content);
            ast = parse(id, content);
            Index aggregate = ast.nodes.data[2].variable.expr;
            expect(ast.nodes.kind[aggregate] == NODE_ENUM_TWO);
            Index start = ast.nodes.data[aggregate].aggregate.start;
            Index end = ast.nodes.data[aggregate].aggregate.end;
            expect(start == 0);
            expect(end == 0);
        }

        it("should parse an enum with one variant")
        {
            stringview content = stringview_from_cstr("foo :: enum {hello = 1}");
            // stringview content = stringview_from_cstr("foo :: enum {hello}");
            FileId id = add_file("test.wave", content);
            ast = parse(id, content);
            Index aggregate = ast.nodes.data[2].variable.expr;
            expect(ast.nodes.kind[aggregate] == NODE_ENUM_TWO);
            Index start = ast.nodes.data[aggregate].aggregate.start;
            Index end = ast.nodes.data[aggregate].aggregate.end;
            expect(end - start == 0);
            expect(ast.nodes.kind[start] == NODE_VARIANT_SIMPLE);
            Index expr = ast.nodes.data[start].binary.lhs;
            expect(ast.nodes.kind[expr] == NODE_INT);
        }

        it("should parse an enum with two variants")
        {
            stringview content = stringview_from_cstr("foo :: enum {hello(int)\n world}");
            // stringview content = stringview_from_cstr("foo :: enum {hello()\n world}");
            FileId id = add_file("test.wave", content);
            ast = parse(id, content);
            Index aggregate = ast.nodes.data[2].variable.expr;
            expect(ast.nodes.kind[aggregate] == NODE_ENUM_TWO);
            Index start = ast.nodes.data[aggregate].aggregate.start;
            Index end = ast.nodes.data[aggregate].aggregate.end;
            expect(end - start == 1);
            expect(ast.nodes.kind[start] == NODE_VARIANT_TWO);
            expect(ast.nodes.kind[end] == NODE_VARIANT_SIMPLE);

            Index field = ast.nodes.data[start].range.start;
            Index type = ast.nodes.data[field].binary.lhs;
            Index rhs = ast.nodes.data[field].binary.rhs;
            expect(ast.nodes.kind[type] == NODE_IDENTIFIER);
            expect(rhs == 0);
        }

        it("should parse an enum with multiple variants")
        {
            stringview content = stringview_from_cstr("foo :: enum {hello,\n world\nto\nall\nof\nyou}");
            FileId id = add_file("test.wave", content);
            ast = parse(id, content);
            Index aggregate = ast.nodes.data[2].variable.expr;
            expect(ast.nodes.kind[aggregate] == NODE_ENUM);
            Index start = ast.nodes.data[aggregate].aggregate.start;
            Index end = ast.nodes.data[aggregate].aggregate.end;
            int count = 1;
            for (Index i = start; i <= end; ++i) {
                char const *str = strf("%d field", count);
                expect_str(ast.nodes.kind[i] == NODE_VARIANT_SIMPLE, str);
                xfree(str);
                count++;
            }
        }

        it("should parse a simple import")
        {
            stringview content = stringview_from_cstr("import foo");
            FileId id = add_file("test.wave", content);
            ast = parse(id, content);
            Index import = 1;
            expect(ast.nodes.kind[import] == NODE_IMPORT);
            expect(ast.nodes.token[import] == 1);
        }

        it("should parse an import with as")
        {
            stringview content = stringview_from_cstr("import foo as bar");
            FileId id = add_file("test.wave", content);
            ast = parse(id, content);
            Index import = 1;
            expect(ast.nodes.kind[import] == NODE_IMPORT);
            expect(ast.nodes.token[import] == 1);
            expect(ast.nodes.data[import].binary.lhs == 3);
        }

        it("should parse a complex import")
        {
            stringview content = stringview_from_cstr("import foo { baz, fizzbuzz } as bar");
            FileId id = add_file("test.wave", content);
            ast = parse(id, content);
            Index import = 1;
            expect(ast.nodes.kind[import] == NODE_IMPORT_COMPLEX);
            expect(ast.nodes.token[import] == 1);
            Index index = ast.nodes.data[import].binary.rhs;
            expect(ast.nodes.kind[index] == NODE_RANGE);
            Range range = ast.nodes.data[index].range;
            Index start = range.start;
            Index end = range.end;
            int count = 1;
            for (Index i = start; i <= end; ++i) {
                char const *str = strf("%d symbol", count);
                expect_str(ast.nodes.kind[i] == NODE_IDENTIFIER, str);
                xfree(str);
                count++;
            }
        }

        it("should parse an import with { ... }")
        {
            stringview content = stringview_from_cstr("import foo { ... }");
            FileId id = add_file("test.wave", content);
            ast = parse(id, content);
            Index import = 1;
            expect(ast.nodes.kind[import] == NODE_IMPORT_COMPLEX);
            expect(ast.nodes.token[import] == 1);
            Index index = ast.nodes.data[import].binary.rhs;
            expect(ast.nodes.kind[index] == NODE_ALL_SYMBOLS);
        }

        it("should parse a simple foreign import")
        {
            stringview content = stringview_from_cstr("foreign import foo");
            FileId id = add_file("test.wave", content);
            ast = parse(id, content);
            Index import = 1;
            expect(ast.nodes.kind[import] == NODE_FOREIGN_IMPORT);
            expect(ast.nodes.token[import] == 2);
        }

        it("should parse a foreign import with as")
        {
            stringview content = stringview_from_cstr("foreign import foo as bar");
            FileId id = add_file("test.wave", content);
            ast = parse(id, content);
            Index import = 1;
            expect(ast.nodes.kind[import] == NODE_FOREIGN_IMPORT);
            expect(ast.nodes.token[import] == 2);
            expect(ast.nodes.data[import].binary.lhs == 4);
        }

        it("should parse a complex foreign import")
        {
            stringview content = stringview_from_cstr("foreign import foo { baz, fizzbuzz } as bar");
            FileId id = add_file("test.wave", content);
            ast = parse(id, content);
            Index import = 1;
            expect(ast.nodes.kind[import] == NODE_FOREIGN_IMPORT_COMPLEX);
            expect(ast.nodes.token[import] == 2);
            Index index = ast.nodes.data[import].binary.rhs;
            expect(ast.nodes.kind[index] == NODE_RANGE);
            Range range = ast.nodes.data[index].range;
            Index start = range.start;
            Index end = range.end;
            int count = 1;
            for (Index i = start; i <= end; ++i) {
                char const *str = strf("%d symbol", count);
                expect_str(ast.nodes.kind[i] == NODE_IDENTIFIER, str);
                xfree(str);
                count++;
            }
        }

        it("should parse a foreign import with { ... }")
        {
            stringview content = stringview_from_cstr("foreign import foo { ... }");
            FileId id = add_file("test.wave", content);
            ast = parse(id, content);
            Index import = 1;
            expect(ast.nodes.kind[import] == NODE_FOREIGN_IMPORT_COMPLEX);
            expect(ast.nodes.token[import] == 2);
            Index index = ast.nodes.data[import].binary.rhs;
            expect(ast.nodes.kind[index] == NODE_ALL_SYMBOLS);
        }

        it("should parse some math expressions")
        {
            stringview content = stringview_from_cstr("hello :: 2 * 1 - 2 * 3");
            FileId id = add_file("test.wave", content);
            ast = parse(id, content);
            stringview str = print_ast(ast);
            stringview expected = stringview_from_cstr("(def hello (- (* 2 1) (* 2 3)))");
            expect(stringview_cmp(str, expected));
        }
    }

    describe("Diagnostics")
    {
        skip("should print a diagnostic")
        {
            char const *hint = "I can only comprehend these three type of declaration:\n\n"
                               "    foo : int : 5\n"
                               "    bar := 5\n"
                               "    baz :: (integer: int) -> int {\n"
                               "        return integer\n"
                               "    }\n\n"
                               "I can also comprehend the when directive\n\n"
                               "    when true {\n"
                               "        var :: 5\n"
                               "    }\n\n"
                               "Try writing one of these";
            stringview content = stringview_from_cstr("main :: (args: []string) -> void {\n    println(\"Hello, World!\")\n}\n");
            FileId file_id = add_file("example.txt", content);
            array(Diagnostic) diags = NULL;
            Span span = { .file_id = file_id, .start = 0, .end = 0 };
            array_push(diags, error(span, "unused variable", "unused", hint));
            emit_diagnostics(diags);
            vfs_cleanup();
        }
    }
}
