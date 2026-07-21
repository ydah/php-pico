#ifndef PPHP_AST_H
#define PPHP_AST_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "lexer.h"

enum {
    PC_MOD_PUBLIC = 1U << 0,
    PC_MOD_PROTECTED = 1U << 1,
    PC_MOD_PRIVATE = 1U << 2,
    PC_MOD_STATIC = 1U << 3,
    PC_MOD_ABSTRACT = 1U << 4,
    PC_MOD_FINAL = 1U << 5,
    PC_MOD_READONLY = 1U << 6,
    PC_MOD_INTERFACE = 1U << 7
};

typedef enum pc_ast_kind {
    AST_PROGRAM = 0,
    AST_BLOCK,
    AST_NULL,
    AST_BOOL,
    AST_INT,
    AST_FLOAT,
    AST_STRING,
    AST_VARIABLE,
    AST_IDENTIFIER,
    AST_UNARY,
    AST_BINARY,
    AST_ASSIGN,
    AST_TERNARY,
    AST_MATCH,
    AST_MATCH_ARM,
    AST_CALL,
    AST_INDEX,
    AST_MEMBER,
    AST_ARRAY,
    AST_ARRAY_ITEM,
    AST_EXPR_STMT,
    AST_ECHO,
    AST_IF,
    AST_WHILE,
    AST_DO_WHILE,
    AST_FOR,
    AST_FOREACH,
    AST_SWITCH,
    AST_CASE,
    AST_BREAK,
    AST_CONTINUE,
    AST_RETURN,
    AST_THROW,
    AST_GLOBAL,
    AST_STATIC,
    AST_CONST,
    AST_FUNCTION,
    AST_CLOSURE,
    AST_PARAM,
    AST_INCLUDE,
    AST_UNSET,
    AST_ISSET,
    AST_EMPTY,
    AST_CLASS,
    AST_PROPERTY,
    AST_NEW,
    AST_TRY,
    AST_CATCH
} pc_ast_kind;

typedef struct pc_ast pc_ast;

struct pc_ast {
    pc_ast_kind kind;
    uint32_t line;
    pc_ast *next;
    union {
        struct {
            pc_token token;
        } literal;
        struct {
            pc_token name;
        } named;
        struct {
            pc_token_type op;
            pc_ast *operand;
            int postfix;
        } unary;
        struct {
            pc_token_type op;
            pc_ast *left;
            pc_ast *right;
        } binary;
        struct {
            pc_ast *condition;
            pc_ast *then_expr;
            pc_ast *else_expr;
        } ternary;
        struct {
            pc_ast *subject;
            pc_ast *arms;
        } match_expr;
        struct {
            pc_ast *conditions;
            pc_ast *result;
        } match_arm;
        struct {
            pc_ast *callee;
            pc_ast *arguments;
            size_t count;
        } call;
        struct {
            pc_ast *base;
            pc_ast *key;
        } index;
        struct {
            pc_ast *base;
            pc_ast *dynamic_name;
            pc_token name;
            pc_token_type op;
        } member;
        struct {
            pc_ast *items;
            size_t count;
        } list;
        struct {
            pc_ast *key;
            pc_ast *value;
            int spread;
        } array_item;
        struct {
            pc_ast *expression;
        } expression;
        struct {
            pc_ast *condition;
            pc_ast *then_branch;
            pc_ast *else_branch;
        } if_stmt;
        struct {
            pc_ast *condition;
            pc_ast *body;
        } loop;
        struct {
            pc_ast *initializers;
            pc_ast *conditions;
            pc_ast *increments;
            pc_ast *body;
        } for_stmt;
        struct {
            pc_ast *iterable;
            pc_ast *key;
            pc_ast *value;
            pc_ast *body;
        } foreach_stmt;
        struct {
            pc_ast *subject;
            pc_ast *cases;
        } switch_stmt;
        struct {
            pc_ast *condition;
            pc_ast *body;
        } case_stmt;
        struct {
            unsigned level;
        } jump;
        struct {
            pc_token name;
            pc_ast *value;
        } binding;
        struct {
            pc_token name;
            pc_ast *parameters;
            pc_ast *body;
            size_t parameter_count;
            uint8_t flags;
            int declaration_only;
        } function;
        struct {
            pc_ast *parameters;
            pc_ast *captures;
            pc_ast *body;
            size_t parameter_count;
            int is_arrow;
            int is_static;
        } closure;
        struct {
            pc_token name;
            pc_ast *default_value;
            int variadic;
            uint8_t flags;
        } parameter;
        struct {
            pc_token_type mode;
            pc_ast *path;
        } include_stmt;
        struct {
            pc_token name;
            pc_token parent;
            pc_ast *interfaces;
            pc_ast *members;
            uint8_t flags;
        } class_decl;
        struct {
            pc_token name;
            pc_ast *default_value;
            uint8_t flags;
        } property;
        struct {
            pc_ast *class_name;
            pc_ast *arguments;
            size_t count;
        } new_expr;
        struct {
            pc_ast *try_block;
            pc_ast *catches;
            pc_ast *finally_block;
        } try_stmt;
        struct {
            pc_ast *types;
            pc_token variable;
            pc_ast *body;
        } catch_stmt;
    } as;
};

typedef struct pc_arena_block pc_arena_block;

typedef struct pc_arena {
    pc_arena_block *blocks;
    size_t block_size;
} pc_arena;

void pc_arena_init(pc_arena *arena, size_t block_size);
void pc_arena_destroy(pc_arena *arena);
void *pc_arena_alloc(pc_arena *arena, size_t size);
pc_ast *pc_ast_new(pc_arena *arena, pc_ast_kind kind, uint32_t line);
void pc_ast_append(pc_ast **head, pc_ast **tail, pc_ast *node);
const char *pc_ast_kind_name(pc_ast_kind kind);
void pc_ast_dump(FILE *stream, const pc_ast *node);

#endif
