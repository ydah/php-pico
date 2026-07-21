#include "ast.h"

#include "pphp/pphp.h"

#include <string.h>

struct pc_arena_block {
    pc_arena_block *next;
    size_t used;
    size_t capacity;
    unsigned char data[];
};

static size_t align_size(size_t size) {
    const size_t alignment = sizeof(void *);
    return (size + alignment - 1U) & ~(alignment - 1U);
}

void pc_arena_init(pc_arena *arena, size_t block_size) {
    arena->blocks = NULL;
    arena->block_size = block_size < 1024U ? 1024U : block_size;
}

void pc_arena_destroy(pc_arena *arena) {
    pc_arena_block *block = arena->blocks;
    while (block != NULL) {
        pc_arena_block *next = block->next;
        pphp_free(block);
        block = next;
    }
    arena->blocks = NULL;
}

void *pc_arena_alloc(pc_arena *arena, size_t size) {
    pc_arena_block *block;
    size_t aligned = align_size(size);
    if (aligned < size) {
        return NULL;
    }
    block = arena->blocks;
    if (block == NULL || block->capacity - block->used < aligned) {
        size_t capacity = arena->block_size;
        pc_arena_block *created;
        if (capacity < aligned) {
            capacity = aligned;
        }
        created = pphp_alloc(sizeof(*created) + capacity);
        if (created == NULL) {
            return NULL;
        }
        created->next = block;
        created->used = 0U;
        created->capacity = capacity;
        arena->blocks = created;
        block = created;
    }
    {
        void *result = block->data + block->used;
        block->used += aligned;
        memset(result, 0, size);
        return result;
    }
}

pc_ast *pc_ast_new(pc_arena *arena, pc_ast_kind kind, uint32_t line) {
    pc_ast *node = pc_arena_alloc(arena, sizeof(*node));
    if (node == NULL) {
        return NULL;
    }
    node->kind = kind;
    node->line = line;
    return node;
}

void pc_ast_append(pc_ast **head, pc_ast **tail, pc_ast *node) {
    if (node == NULL) {
        return;
    }
    if (*tail == NULL) {
        *head = node;
    } else {
        (*tail)->next = node;
    }
    while (node->next != NULL) {
        node = node->next;
    }
    *tail = node;
}

const char *pc_ast_kind_name(pc_ast_kind kind) {
    static const char *const names[] = {
        "PROGRAM", "BLOCK", "NULL", "BOOL", "INT", "FLOAT", "STRING",
        "VARIABLE", "IDENTIFIER", "UNARY", "BINARY", "ASSIGN", "TERNARY",
        "MATCH", "MATCH_ARM", "CALL", "INDEX", "MEMBER", "ARRAY", "ARRAY_ITEM", "EXPR_STMT",
        "ECHO", "IF", "WHILE", "DO_WHILE", "FOR", "FOREACH", "SWITCH",
        "CASE", "BREAK", "CONTINUE", "RETURN", "THROW", "GLOBAL", "STATIC",
        "CONST", "FUNCTION", "CLOSURE", "PARAM", "INCLUDE", "UNSET", "ISSET", "EMPTY",
        "CLASS", "PROPERTY", "NEW", "TRY", "CATCH"
    };
    if ((size_t)kind >= sizeof(names) / sizeof(names[0])) {
        return "UNKNOWN";
    }
    return names[kind];
}

static void indent(FILE *stream, unsigned depth) {
    unsigned i;
    for (i = 0U; i < depth; i++) {
        fputs("  ", stream);
    }
}

static void dump_token(FILE *stream, pc_token token) {
    fprintf(stream, " %.*s", (int)token.length, token.start);
}

static void dump_node(FILE *stream, const pc_ast *node, unsigned depth);

static void dump_list(FILE *stream, const pc_ast *node, unsigned depth) {
    while (node != NULL) {
        dump_node(stream, node, depth);
        node = node->next;
    }
}

static void dump_node(FILE *stream, const pc_ast *node, unsigned depth) {
    if (node == NULL) {
        return;
    }
    indent(stream, depth);
    fprintf(stream, "%s", pc_ast_kind_name(node->kind));
    switch (node->kind) {
        case AST_INT:
        case AST_FLOAT:
        case AST_STRING:
        case AST_BOOL:
        case AST_VARIABLE:
        case AST_IDENTIFIER:
            dump_token(stream, node->as.literal.token);
            break;
        case AST_UNARY:
            fprintf(stream, " %s%s", pc_token_name(node->as.unary.op),
                    node->as.unary.postfix ? " (postfix)" : "");
            break;
        case AST_BINARY:
        case AST_ASSIGN:
            fprintf(stream, " %s", pc_token_name(node->as.binary.op));
            break;
        case AST_MEMBER:
            fprintf(stream, " %s", pc_token_name(node->as.member.op));
            dump_token(stream, node->as.member.name);
            break;
        case AST_BREAK:
        case AST_CONTINUE:
            fprintf(stream, " %u", node->as.jump.level);
            break;
        case AST_FUNCTION:
            dump_token(stream, node->as.function.name);
            break;
        case AST_PARAM:
            dump_token(stream, node->as.parameter.name);
            break;
        case AST_CLASS:
            dump_token(stream, node->as.class_decl.name);
            break;
        case AST_PROPERTY:
            dump_token(stream, node->as.property.name);
            break;
        case AST_CONST:
        case AST_STATIC:
            dump_token(stream, node->as.binding.name);
            break;
        default:
            break;
    }
    fprintf(stream, " @%u\n", node->line);
    switch (node->kind) {
        case AST_PROGRAM:
        case AST_BLOCK:
        case AST_ARRAY:
        case AST_ECHO:
        case AST_GLOBAL:
            dump_list(stream, node->as.list.items, depth + 1U);
            break;
        case AST_UNARY:
        case AST_UNSET:
        case AST_ISSET:
        case AST_EMPTY:
            dump_node(stream, node->as.unary.operand, depth + 1U);
            break;
        case AST_BINARY:
        case AST_ASSIGN:
            dump_node(stream, node->as.binary.left, depth + 1U);
            dump_node(stream, node->as.binary.right, depth + 1U);
            break;
        case AST_TERNARY:
            dump_node(stream, node->as.ternary.condition, depth + 1U);
            dump_node(stream, node->as.ternary.then_expr, depth + 1U);
            dump_node(stream, node->as.ternary.else_expr, depth + 1U);
            break;
        case AST_MATCH:
            dump_node(stream, node->as.match_expr.subject, depth + 1U);
            dump_list(stream, node->as.match_expr.arms, depth + 1U);
            break;
        case AST_MATCH_ARM:
            dump_list(stream, node->as.match_arm.conditions, depth + 1U);
            dump_node(stream, node->as.match_arm.result, depth + 1U);
            break;
        case AST_CALL:
            dump_node(stream, node->as.call.callee, depth + 1U);
            dump_list(stream, node->as.call.arguments, depth + 1U);
            break;
        case AST_INDEX:
            dump_node(stream, node->as.index.base, depth + 1U);
            dump_node(stream, node->as.index.key, depth + 1U);
            break;
        case AST_MEMBER:
            dump_node(stream, node->as.member.base, depth + 1U);
            break;
        case AST_ARRAY_ITEM:
            dump_node(stream, node->as.array_item.key, depth + 1U);
            dump_node(stream, node->as.array_item.value, depth + 1U);
            break;
        case AST_EXPR_STMT:
        case AST_RETURN:
        case AST_THROW:
            dump_node(stream, node->as.expression.expression, depth + 1U);
            break;
        case AST_IF:
            dump_node(stream, node->as.if_stmt.condition, depth + 1U);
            dump_node(stream, node->as.if_stmt.then_branch, depth + 1U);
            dump_node(stream, node->as.if_stmt.else_branch, depth + 1U);
            break;
        case AST_WHILE:
        case AST_DO_WHILE:
            dump_node(stream, node->as.loop.condition, depth + 1U);
            dump_node(stream, node->as.loop.body, depth + 1U);
            break;
        case AST_FOR:
            dump_list(stream, node->as.for_stmt.initializers, depth + 1U);
            dump_list(stream, node->as.for_stmt.conditions, depth + 1U);
            dump_list(stream, node->as.for_stmt.increments, depth + 1U);
            dump_node(stream, node->as.for_stmt.body, depth + 1U);
            break;
        case AST_FOREACH:
            dump_node(stream, node->as.foreach_stmt.iterable, depth + 1U);
            dump_node(stream, node->as.foreach_stmt.key, depth + 1U);
            dump_node(stream, node->as.foreach_stmt.value, depth + 1U);
            dump_node(stream, node->as.foreach_stmt.body, depth + 1U);
            break;
        case AST_SWITCH:
            dump_node(stream, node->as.switch_stmt.subject, depth + 1U);
            dump_list(stream, node->as.switch_stmt.cases, depth + 1U);
            break;
        case AST_CASE:
            dump_node(stream, node->as.case_stmt.condition, depth + 1U);
            dump_node(stream, node->as.case_stmt.body, depth + 1U);
            break;
        case AST_STATIC:
        case AST_CONST:
            dump_node(stream, node->as.binding.value, depth + 1U);
            break;
        case AST_FUNCTION:
            dump_list(stream, node->as.function.parameters, depth + 1U);
            dump_node(stream, node->as.function.body, depth + 1U);
            break;
        case AST_CLOSURE:
            dump_list(stream, node->as.closure.parameters, depth + 1U);
            dump_list(stream, node->as.closure.captures, depth + 1U);
            dump_node(stream, node->as.closure.body, depth + 1U);
            break;
        case AST_PARAM:
            dump_node(stream, node->as.parameter.default_value, depth + 1U);
            break;
        case AST_INCLUDE:
            dump_node(stream, node->as.include_stmt.path, depth + 1U);
            break;
        case AST_CLASS:
            dump_list(stream, node->as.class_decl.members, depth + 1U);
            break;
        case AST_PROPERTY:
            dump_node(stream, node->as.property.default_value, depth + 1U);
            break;
        case AST_NEW:
            dump_node(stream, node->as.new_expr.class_name, depth + 1U);
            dump_list(stream, node->as.new_expr.arguments, depth + 1U);
            break;
        case AST_TRY:
            dump_node(stream, node->as.try_stmt.try_block, depth + 1U);
            dump_list(stream, node->as.try_stmt.catches, depth + 1U);
            dump_node(stream, node->as.try_stmt.finally_block, depth + 1U);
            break;
        case AST_CATCH:
            dump_list(stream, node->as.catch_stmt.types, depth + 1U);
            dump_node(stream, node->as.catch_stmt.body, depth + 1U);
            break;
        default:
            break;
    }
}

void pc_ast_dump(FILE *stream, const pc_ast *node) {
    dump_node(stream, node, 0U);
}
