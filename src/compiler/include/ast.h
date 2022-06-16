#ifndef AST_H_
#define AST_H_

#include "diagnostic.h"
#include "lexer.h"
#include "util.h"

// Extra data macros
#define add_extra(_buf, _data) \
    (__add_extra((char **)&(_buf), (void *)&(_data), sizeof((_data))))

#define get_extra(_buf, _type, _index) \
    (*((_type *)((_buf) + (_index))))

/// This represents an index into:
///   - the token array
///   - the source string
typedef uint32_t Index;

// NOTE: We don't use x-macros because they aren't really usable with comments

/// The type of a Node
/// NOTE: Generic means that you have to access the generic_decl field of Data
typedef enum NodeKind {
    NODE_INVALID = -1,
    NODE_ROOT,

    // Generic decl
    NODE_GENERIC,
    // GenericOne decl
    NODE_GENERIC_ONE,

    // token : type : expr
    NODE_CONST,
    // token : type = expr
    NODE_VAR,

    // token { lhs, rhs }
    NODE_STRUCT_TWO,
    // token { start..end }
    NODE_STRUCT,

    // lhs : rhs
    NODE_FIELD,

    // token { lhs, rhs }
    NODE_ENUM_TWO,
    // token { start..end }
    NODE_ENUM,

    // lhs = rhs
    NODE_VARIANT_SIMPLE,
    // lhs(rhs)
    // where rhs = range of types => start..end
    NODE_VARIANT_UNNAMED,
    // lhs(rhs)
    // where rhs = range of fields => start..end
    NODE_VARIANT_NAMED,

    // (args) -> return_type "calling_convention"
    NODE_FUNC_PROTO,
    NODE_FUNC_PROTO_ONE,

    // token : type = expr
    NODE_PARAM,
    // token : ... type
    NODE_VARARG,

    // fn-proto body
    NODE_FUNC,

    // token { start..end }
    NODE_FOREIGN,

    // @token(range) postfix
    NODE_COMPTIME,

    // token expr body
    NODE_IF_SIMPLE,
    // token expr then else
    NODE_IF,

    // token expr body
    NODE_FOR,

    // token expr body
    NODE_MATCH,
    // expr => body
    NODE_MATCH_CASE,

    // token stmt
    NODE_DEFER,

    // token expr
    NODE_RETURN,

    // token label
    NODE_BREAK,
    NODE_CONTINUE,

    // using start, end
    NODE_USING_SIMPLE,
    // using name: expr (in this case it is a type)
    NODE_USING_TYPE,
    // using name: expr
    NODE_USING_EXPR,

    // range
    NODE_BLOCK,

    // token = literal
    NODE_INT,
    NODE_FLOAT,
    NODE_CHAR,
    NODE_STRING,
    NODE_IDENTIFIER,
    NODE_UNDEF,
    NODE_ENUM_ACCESSOR,

    // token expr
    NODE_NEW_SIMPLE,
    // token (lhs) rhs
    NODE_NEW_ALLOCATOR,
    // token [lhs] rhs
    NODE_NEW_LENGTH,
    // token (lhs.start)[lhs.end] rhs
    NODE_NEW_COMPLEX,

    // [lhs, rhs]
    NODE_ARRAY_TWO,
    // [lhs..rhs]
    NODE_ARRAY,
    // [lhs; rhs]
    NODE_ARRAY_INIT,

    // {"lhs": rhs}
    NODE_MAP_TWO,
    // {lhs..rhs}
    NODE_MAP,
    // "lhs": rhs
    NODE_MAP_ITEM,

    // lhs '..' rhs
    NODE_RANGE,

    NODE_IF_EXPR,
    NODE_MATCH_EXPR,

    NODE_OR,
    NODE_IN_EXPR,
    NODE_AS_EXPR,
    NODE_PIPE_EXPR,

    // binary expressions: lhs token rhs
    NODE_OR_EXPR,
    NODE_AND_EXPR,
    NODE_EQ_EXPR,
    NODE_NOTEQ_EXPR,
    NODE_LT_EXPR,
    NODE_GT_EXPR,
    NODE_LTEQ_EXPR,
    NODE_GTEQ_EXPR,
    NODE_MUL_EXPR,
    NODE_DIV_EXPR,
    NODE_MOD_EXPR,
    NODE_BITAND_EXPR,
    NODE_ADD_EXPR,
    NODE_SUB_EXPR,
    NODE_BITOR_EXPR,
    NODE_BITXOR_EXPR,
    NODE_LSHIFT_EXPR,
    NODE_RSHIFT_EXPR,

    // unary expressions: token lhs
    NODE_UNARY_PLUS,
    NODE_UNARY_MINUS,
    NODE_BITNOT,
    NODE_UNARY_NOT,
    NODE_REF,
    NODE_MUT_REF,
    NODE_DEREF,
    NODE_TYPEOF,
    NODE_SIZEOF,
    NODE_ALIGNOF,
    NODE_OFFSETOF,

    // token(lhs, rhs)
    NODE_CALL_TWO,
    // token(lhs..rhs)
    NODE_CALL,
    // token<lhs>(rhs)
    // where:
    //   lhs = range of types => start..end
    //   rhs = range of args => start..end
    NODE_CALL_GENERIC,

    // lhs: rhs
    NODE_ARG,

    // lhs.rhs
    NODE_FIELD_ACCESS,

    // lhs[rhs]
    NODE_ARRAY_ACCESS,

    // Types
    // token lhs
    NODE_REF_TYPE,
    NODE_REF_MUT_TYPE,
    NODE_REF_OWN_TYPE,
    // token lhs ] rhs
    NODE_ARRAY_TYPE,
    // token [lhs] rhs
    NODE_MAP_TYPE,
} NodeKind;

/// A slice of data in the extra array
typedef struct Range {
    uint32_t start;
    uint32_t end;
} Range;

/// The data associated with every Node
/// In the zig compiler it is a simple struct lhs and rhs. This union is more descriptive.
typedef union Data {
    struct {
        Index params;
        Index postfix; // This can be a decl, a stmt or an expr
    } comptime;
    struct {
        Index info;
        Index decl;
    } generic_decl;
    struct {
        Index type;
        Index expr; // struct, enum, func, type
    } variable;
    struct {
        Index func_proto; // Index into extra array
        Index body;
    } func;
    struct {
        Index proto; // FuncProto struct
        Index return_type;
    } func_proto;
    struct {
        Index type;
        Index expr; // default value
    } param;
    struct {
        Index start;
        Index end;
    } simple_using;
    struct {
        Index name;
        Index expr; // NOTE: This can be both an expression or a type
    } complex_using;
    struct {
        Index expr;
        Index body;
    } control_flow; // if, for and match
    struct {
        Index label;
    } loop_modifiers; // continue and break
    struct {
        Index expr;
    } unary;
    struct {
        Index lhs;
        Index rhs;
    } binary;
    Range range;
    Range block;
    Index index; // just to index other nodes
    Index literal;
} Data;

/// The bodies of the branches of ifs
typedef struct If {
    Index then;
    Index body;
} If;

/// A function prototype with only one parameter
typedef struct FuncProtoOne {
    Index param;
    Index calling_convention;
} FuncProtoOne;

/// A function prototype with multiple parameters
typedef struct FuncProto {
    Range params;
    Index calling_convention;
} FuncProto;

/// Generic Declaration informations (only one type parameter)
typedef struct GenericOne {
    Index type_param;
    Range where_block;
} GenericOne;

/// Generic Declaration informations (multiple type parameters)
typedef struct Generic {
    Range type_params;
    Range where_block; // Inlined Data
} Generic;

/// The actual syntax tree
/// This struct is implemented as an SOA (structure of arrays) for performance and cache coherency
typedef struct NodeList {
    array(NodeKind) kind;
    array(Data) data;
    array(Index) token;
    array(char) extra; // used to store more complex type. NOTE: This uses type punning. Be careful!
} NodeList;

/// The result of parse()
typedef struct Ast {
    stringview src;
    array(TokenKind) token_kind;
    array(uint32_t) token_start;
    NodeList nodes;
    array(Index) decls;
    array(Diagnostic) diagnostics;
} Ast;

/// Free the allocated memory of an Ast
void free_ast(Ast ast);

/// Add some data to NodeList.extra
uint32_t __add_extra(char **, void *data, size_t len);

#endif // AST_H_
