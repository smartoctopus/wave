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
                const char *str1 = token_to_string(lexed_src.kind[i]);
                const char *str2 = token_to_string(kinds[i]);
                const char *str = strf("%d) %s = %s", i, str1, str2);
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
            free_ast(ast);
        }

        it("should parse an empty string")
        {
            ast = parse(0, stringview_from_cstr(""));
            expect(ast.nodes.kind[0] == NODE_ROOT);
            expect(array_length(ast.nodes.kind) == 1);
        }

        it("should parse a function without parameters")
        {
            ast = parse(0, stringview_from_cstr("main :: () {\n}"));
            expect(ast.nodes.kind[1] == NODE_CONST);
            Index func = ast.nodes.data[1].variable.expr;
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
            // FIXME: when we can parse the types check them
            stringview content = stringview_from_cstr("main :: (args: ) {\n}\n");
            ast = parse(0, content);
            Index func = ast.nodes.data[1].variable.expr;
            Index index = ast.nodes.data[func].func.func_proto;
            Index proto_index = ast.nodes.data[index].func_proto.proto;
            FuncProtoOne func_proto = get_extra(ast.nodes.extra, FuncProtoOne, proto_index);
            Index param = func_proto.param;
            expect(ast.nodes.kind[param] == NODE_PARAM);
            expect(ast.nodes.data[param].param.type == 0);
            expect(ast.nodes.data[param].param.expr == 0);
        }

        it("should parse a function with multple parameters")
        {
            // FIXME: when we can parse the types check them
            stringview content = stringview_from_cstr("main :: (arg1: , arg2: , arg3: , arg4: , arg5: , arg6: ) {\n}\n");
            ast = parse(0, content);
            Index func = ast.nodes.data[1].variable.expr;
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
                expect_str(ast.nodes.data[i].param.type == 0, type_str);
                expect_str(ast.nodes.data[i].param.expr == 0, expr_str);
                xfree(kind_str);
                xfree(type_str);
                xfree(expr_str);

                count++;
            }
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
