#include "parser.h"

#include "pphp/pphp_config.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

enum {
    PREC_NONE = 0,
    PREC_OR,
    PREC_XOR,
    PREC_AND,
    PREC_ASSIGN,
    PREC_TERNARY,
    PREC_COALESCE,
    PREC_BOOL_OR,
    PREC_BOOL_AND,
    PREC_BIT_OR,
    PREC_BIT_XOR,
    PREC_BIT_AND,
    PREC_EQUALITY,
    PREC_RELATIONAL,
    PREC_CONCAT,
    PREC_SHIFT,
    PREC_ADDITIVE,
    PREC_MULTIPLICATIVE,
    PREC_INSTANCEOF,
    PREC_UNARY,
    PREC_POWER,
    PREC_POSTFIX
};

static void advance_token(pc_parser *parser) {
    parser->previous = parser->current;
    for (;;) {
        parser->current = pc_lexer_next(&parser->lexer);
        if (parser->current.type != T_ERROR) {
            return;
        }
        if (!parser->failed) {
            (void)snprintf(parser->error, sizeof(parser->error), "%s",
                           pc_lexer_error(&parser->lexer));
            parser->failed = 1;
        }
        return;
    }
}

static void fail_at(pc_parser *parser, pc_token token, const char *format, ...) {
    va_list arguments;
    if (parser->failed) {
        return;
    }
    parser->previous = token;
    va_start(arguments, format);
    (void)vsnprintf(parser->error, sizeof(parser->error), format, arguments);
    va_end(arguments);
    parser->failed = 1;
}

static int check(const pc_parser *parser, pc_token_type type) {
    return parser->current.type == type;
}

static int match(pc_parser *parser, pc_token_type type) {
    if (!check(parser, type)) {
        return 0;
    }
    advance_token(parser);
    return 1;
}

static pc_token consume(pc_parser *parser, pc_token_type type, const char *message) {
    pc_token token = parser->current;
    if (check(parser, type)) {
        advance_token(parser);
        return token;
    }
    fail_at(parser, parser->current, "%s; got %s", message,
            pc_token_name(parser->current.type));
    return token;
}

static pc_ast *new_node(pc_parser *parser, pc_ast_kind kind, uint32_t line) {
    pc_ast *node = pc_ast_new(parser->arena, kind, line);
    if (node == NULL) {
        fail_at(parser, parser->current, "out of memory while building AST");
    }
    return node;
}

static int enter_depth(pc_parser *parser) {
    parser->depth++;
    if (parser->depth > PPHP_PARSE_DEPTH_MAX) {
        fail_at(parser, parser->current, "parser nesting exceeds limit %u",
                (unsigned)PPHP_PARSE_DEPTH_MAX);
        return 0;
    }
    return 1;
}

static void leave_depth(pc_parser *parser) {
    if (parser->depth != 0U) {
        parser->depth--;
    }
}

static int precedence(pc_token_type type) {
    switch (type) {
        case T_OR: return PREC_OR;
        case T_XOR: return PREC_XOR;
        case T_AND: return PREC_AND;
        case T_EQUAL:
        case T_PLUS_EQUAL:
        case T_MINUS_EQUAL:
        case T_STAR_EQUAL:
        case T_SLASH_EQUAL:
        case T_DOT_EQUAL:
        case T_PERCENT_EQUAL:
        case T_POW_EQUAL:
        case T_AMP_EQUAL:
        case T_PIPE_EQUAL:
        case T_CARET_EQUAL:
        case T_SHIFT_LEFT_EQUAL:
        case T_SHIFT_RIGHT_EQUAL:
        case T_COALESCE_EQUAL:
            return PREC_ASSIGN;
        case T_QUESTION: return PREC_TERNARY;
        case T_COALESCE: return PREC_COALESCE;
        case T_BOOL_OR: return PREC_BOOL_OR;
        case T_BOOL_AND: return PREC_BOOL_AND;
        case T_PIPE: return PREC_BIT_OR;
        case T_CARET: return PREC_BIT_XOR;
        case T_AMP: return PREC_BIT_AND;
        case T_EQUAL_EQUAL:
        case T_NOT_EQUAL:
        case T_IDENTICAL:
        case T_NOT_IDENTICAL:
        case T_SPACESHIP:
            return PREC_EQUALITY;
        case T_LT:
        case T_LT_EQUAL:
        case T_GT:
        case T_GT_EQUAL:
            return PREC_RELATIONAL;
        case T_DOT: return PREC_CONCAT;
        case T_SHIFT_LEFT:
        case T_SHIFT_RIGHT:
            return PREC_SHIFT;
        case T_PLUS:
        case T_MINUS:
            return PREC_ADDITIVE;
        case T_STAR:
        case T_SLASH:
        case T_PERCENT:
            return PREC_MULTIPLICATIVE;
        case T_INSTANCEOF: return PREC_INSTANCEOF;
        case T_POW: return PREC_POWER;
        case T_LPAREN:
        case T_LBRACKET:
        case T_ARROW:
        case T_NULLSAFE_ARROW:
        case T_SCOPE:
        case T_PLUS_PLUS:
        case T_MINUS_MINUS:
            return PREC_POSTFIX;
        default:
            return PREC_NONE;
    }
}

static int is_assignment(pc_token_type type) {
    return precedence(type) == PREC_ASSIGN;
}

static int is_lvalue(const pc_ast *node) {
    return node != NULL && (node->kind == AST_VARIABLE || node->kind == AST_INDEX ||
                            node->kind == AST_MEMBER);
}

static pc_ast *parse_expression_precedence(pc_parser *parser, int minimum);
static pc_ast *parse_call(pc_parser *parser, pc_ast *callee, uint32_t line);
static pc_ast *parse_block(pc_parser *parser, uint32_t line);
static void parse_optional_type(pc_parser *parser);
static pc_ast *literal_node(pc_parser *parser, pc_ast_kind kind,
                            pc_token token);

static pc_ast *parse_match(pc_parser *parser, pc_token keyword) {
    pc_ast *node = new_node(parser, AST_MATCH, keyword.line);
    pc_ast *arms = NULL;
    pc_ast *arm_tail = NULL;
    int saw_default = 0;
    (void)consume(parser, T_LPAREN, "expected '(' after match");
    if (node != NULL) {
        node->as.match_expr.subject = parse_expression_precedence(
            parser, PREC_NONE + 1);
    }
    (void)consume(parser, T_RPAREN, "expected ')' after match subject");
    (void)consume(parser, T_LBRACE, "expected '{' after match subject");
    if (!enter_depth(parser)) return node;
    while (!check(parser, T_RBRACE) && !check(parser, T_EOF) && !parser->failed) {
        pc_ast *arm = new_node(parser, AST_MATCH_ARM, parser->current.line);
        pc_ast *conditions = NULL;
        pc_ast *condition_tail = NULL;
        if (match(parser, T_DEFAULT)) {
            if (saw_default) {
                fail_at(parser, parser->previous,
                        "match may only contain one default arm");
            }
            saw_default = 1;
        } else {
            do {
                pc_ast *condition = parse_expression_precedence(
                    parser, PREC_ASSIGN + 1);
                pc_ast_append(&conditions, &condition_tail, condition);
                if (!match(parser, T_COMMA)) break;
            } while (!check(parser, T_DOUBLE_ARROW) && !parser->failed);
        }
        (void)consume(parser, T_DOUBLE_ARROW, "expected '=>' in match arm");
        if (arm != NULL) {
            arm->as.match_arm.conditions = conditions;
            arm->as.match_arm.result = parse_expression_precedence(
                parser, PREC_ASSIGN + 1);
            pc_ast_append(&arms, &arm_tail, arm);
        }
        if (!match(parser, T_COMMA) && !check(parser, T_RBRACE)) {
            fail_at(parser, parser->current,
                    "expected ',' after match arm");
        }
    }
    (void)consume(parser, T_RBRACE, "expected '}' after match arms");
    leave_depth(parser);
    if (node != NULL) node->as.match_expr.arms = arms;
    return node;
}

static pc_ast *parse_closure(pc_parser *parser, pc_token keyword,
                             int is_arrow, int is_static) {
    pc_ast *closure = new_node(parser, AST_CLOSURE, keyword.line);
    pc_ast *parameters = NULL;
    pc_ast *parameter_tail = NULL;
    pc_ast *captures = NULL;
    pc_ast *capture_tail = NULL;
    size_t parameter_count = 0U;
    (void)consume(parser, T_LPAREN, "expected '(' after closure keyword");
    while (!check(parser, T_RPAREN) && !check(parser, T_EOF) && !parser->failed) {
        pc_ast *parameter;
        int variadic;
        if (!check(parser, T_VARIABLE) && !check(parser, T_ELLIPSIS)) {
            parse_optional_type(parser);
        }
        variadic = match(parser, T_ELLIPSIS);
        parameter = new_node(parser, AST_PARAM, parser->current.line);
        if (parameter != NULL) {
            parameter->as.parameter.name = consume(
                parser, T_VARIABLE, "expected closure parameter variable");
            parameter->as.parameter.variadic = variadic;
            if (match(parser, T_EQUAL)) {
                parameter->as.parameter.default_value = parse_expression_precedence(
                    parser, PREC_ASSIGN + 1);
            }
            pc_ast_append(&parameters, &parameter_tail, parameter);
            parameter_count++;
        }
        if (!match(parser, T_COMMA)) break;
    }
    (void)consume(parser, T_RPAREN, "expected ')' after closure parameters");
    if (!is_arrow && match(parser, T_USE)) {
        (void)consume(parser, T_LPAREN, "expected '(' after use");
        while (!check(parser, T_RPAREN) && !check(parser, T_EOF) &&
               !parser->failed) {
            pc_token variable;
            if (match(parser, T_AMP)) {
                fail_at(parser, parser->previous,
                        "closure captures by reference are not supported");
            }
            variable = consume(parser, T_VARIABLE,
                               "expected captured variable");
            if (variable.length == 5U &&
                memcmp(variable.start, "$this", 5U) == 0) {
                fail_at(parser, variable, "$this cannot be listed in use");
            }
            pc_ast_append(&captures, &capture_tail,
                          literal_node(parser, AST_VARIABLE, variable));
            if (!match(parser, T_COMMA)) break;
        }
        (void)consume(parser, T_RPAREN, "expected ')' after closure captures");
    }
    if (match(parser, T_COLON)) parse_optional_type(parser);
    if (closure != NULL) {
        closure->as.closure.parameters = parameters;
        closure->as.closure.captures = captures;
        closure->as.closure.parameter_count = parameter_count;
        closure->as.closure.is_arrow = is_arrow;
        closure->as.closure.is_static = is_static;
    }
    if (is_arrow) {
        (void)consume(parser, T_DOUBLE_ARROW, "expected '=>' after arrow function");
        if (closure != NULL) {
            closure->as.closure.body = parse_expression_precedence(
                parser, PREC_ASSIGN);
        }
    } else {
        (void)consume(parser, T_LBRACE, "expected closure body");
        if (closure != NULL) closure->as.closure.body = parse_block(parser, keyword.line);
    }
    return closure;
}

static pc_ast *literal_node(pc_parser *parser, pc_ast_kind kind, pc_token token) {
    pc_ast *node = new_node(parser, kind, token.line);
    if (node != NULL) {
        node->as.literal.token = token;
    }
    return node;
}

static pc_ast *parse_interpolation(pc_parser *parser, pc_token opening) {
    pc_ast *result = NULL;
    (void)opening;
    while (!parser->failed && !check(parser, T_INTERP_END) && !check(parser, T_EOF)) {
        pc_ast *part = NULL;
        if (check(parser, T_INTERP_PART)) {
            pc_token token = parser->current;
            advance_token(parser);
            part = literal_node(parser, AST_STRING, token);
        } else if (check(parser, T_VARIABLE)) {
            pc_token token = parser->current;
            advance_token(parser);
            part = literal_node(parser, AST_VARIABLE, token);
        } else if (match(parser, T_INTERP_EXPR_START)) {
            part = parse_expression_precedence(parser, PREC_NONE + 1);
            (void)consume(parser, T_INTERP_EXPR_END,
                          "expected '}' after interpolated expression");
        } else {
            fail_at(parser, parser->current, "invalid token in interpolated string");
            break;
        }
        if (result == NULL) {
            result = part;
        } else {
            pc_ast *concat = new_node(parser, AST_BINARY, part != NULL ? part->line : opening.line);
            if (concat == NULL) {
                return result;
            }
            concat->as.binary.op = T_DOT;
            concat->as.binary.left = result;
            concat->as.binary.right = part;
            result = concat;
        }
    }
    (void)consume(parser, T_INTERP_END, "expected end of interpolated string");
    if (result == NULL) {
        pc_token empty = opening;
        empty.length = 0U;
        result = literal_node(parser, AST_STRING, empty);
    }
    return result;
}

static pc_ast *parse_array(pc_parser *parser, uint32_t line) {
    pc_ast *array = new_node(parser, AST_ARRAY, line);
    pc_ast *head = NULL;
    pc_ast *tail = NULL;
    size_t count = 0U;
    if (array == NULL) {
        return NULL;
    }
    while (!check(parser, T_RBRACKET) && !check(parser, T_EOF) && !parser->failed) {
        int spread = match(parser, T_ELLIPSIS);
        pc_ast *first = parse_expression_precedence(parser, PREC_ASSIGN + 1);
        pc_ast *key = NULL;
        pc_ast *value = first;
        pc_ast *item;
        if (match(parser, T_DOUBLE_ARROW)) {
            if (spread) {
                fail_at(parser, parser->previous, "spread array item cannot have a key");
            }
            key = first;
            value = parse_expression_precedence(parser, PREC_ASSIGN + 1);
        }
        item = new_node(parser, AST_ARRAY_ITEM, first != NULL ? first->line : line);
        if (item == NULL) {
            break;
        }
        item->as.array_item.key = key;
        item->as.array_item.value = value;
        item->as.array_item.spread = spread;
        pc_ast_append(&head, &tail, item);
        count++;
        if (!match(parser, T_COMMA)) {
            break;
        }
    }
    (void)consume(parser, T_RBRACKET, "expected ']' after array literal");
    array->as.list.items = head;
    array->as.list.count = count;
    return array;
}

static pc_ast *parse_prefix(pc_parser *parser) {
    pc_token token = parser->current;
    pc_ast *node;
    advance_token(parser);
    switch (token.type) {
        case T_NULL: return new_node(parser, AST_NULL, token.line);
        case T_TRUE:
        case T_FALSE: return literal_node(parser, AST_BOOL, token);
        case T_INTEGER: return literal_node(parser, AST_INT, token);
        case T_FLOAT: return literal_node(parser, AST_FLOAT, token);
        case T_SINGLE_QUOTED:
        case T_DOUBLE_QUOTED:
        case T_HEREDOC:
        case T_NOWDOC:
            return literal_node(parser, AST_STRING, token);
        case T_INTERP_START:
            return parse_interpolation(parser, token);
        case T_VARIABLE:
            return literal_node(parser, AST_VARIABLE, token);
        case T_IDENTIFIER:
        case T_SELF:
        case T_PARENT:
            return literal_node(parser, AST_IDENTIFIER, token);
        case T_STATIC:
            if (check(parser, T_FUNCTION) || check(parser, T_FN)) {
                pc_token closure_keyword = parser->current;
                int is_arrow = closure_keyword.type == T_FN;
                advance_token(parser);
                return parse_closure(parser, closure_keyword, is_arrow, 1);
            }
            return literal_node(parser, AST_IDENTIFIER, token);
        case T_LPAREN:
            if (!enter_depth(parser)) {
                return NULL;
            }
            node = parse_expression_precedence(parser, PREC_NONE + 1);
            (void)consume(parser, T_RPAREN, "expected ')' after expression");
            leave_depth(parser);
            return node;
        case T_LBRACKET:
            return parse_array(parser, token.line);
        case T_MATCH:
            return parse_match(parser, token);
        case T_FUNCTION:
            return parse_closure(parser, token, 0, 0);
        case T_FN:
            return parse_closure(parser, token, 1, 0);
        case T_NEW: {
            pc_token class_token = parser->current;
            pc_ast *class_name;
            pc_ast *call;
            pc_ast *created;
            if (class_token.type != T_IDENTIFIER && class_token.type != T_SELF &&
                class_token.type != T_PARENT && class_token.type != T_STATIC) {
                fail_at(parser, class_token, "expected class name after new");
                return NULL;
            }
            advance_token(parser);
            class_name = literal_node(parser, AST_IDENTIFIER, class_token);
            (void)consume(parser, T_LPAREN, "expected '(' after class name");
            call = parse_call(parser, class_name, token.line);
            created = new_node(parser, AST_NEW, token.line);
            if (created != NULL && call != NULL) {
                created->as.new_expr.class_name = class_name;
                created->as.new_expr.arguments = call->as.call.arguments;
                created->as.new_expr.count = call->as.call.count;
            }
            return created;
        }
        case T_PLUS:
        case T_MINUS:
        case T_BANG:
        case T_TILDE:
        case T_PLUS_PLUS:
        case T_MINUS_MINUS:
        case T_CLONE:
            node = new_node(parser, AST_UNARY, token.line);
            if (node != NULL) {
                node->as.unary.op = token.type;
                node->as.unary.operand = parse_expression_precedence(parser, PREC_UNARY);
                if ((token.type == T_PLUS_PLUS || token.type == T_MINUS_MINUS) &&
                    !is_lvalue(node->as.unary.operand)) {
                    fail_at(parser, token, "increment target is not assignable");
                }
            }
            return node;
        case T_THROW:
            node = new_node(parser, AST_THROW, token.line);
            if (node != NULL) {
                node->as.expression.expression = parse_expression_precedence(parser, PREC_ASSIGN);
            }
            return node;
        case T_ISSET:
        case T_EMPTY:
        case T_UNSET: {
            pc_ast_kind kind = token.type == T_ISSET ? AST_ISSET :
                               (token.type == T_EMPTY ? AST_EMPTY : AST_UNSET);
            node = new_node(parser, kind, token.line);
            (void)consume(parser, T_LPAREN, "expected '(' after language construct");
            if (node != NULL) {
                node->as.unary.operand = parse_expression_precedence(parser, PREC_NONE + 1);
            }
            (void)consume(parser, T_RPAREN, "expected ')' after expression");
            return node;
        }
        case T_PRINT:
        case T_YIELD:
        case T_GOTO:
        case T_NAMESPACE:
        case T_TRAIT:
        case T_ENUM:
        case T_LIST:
            fail_at(parser, token, "unsupported syntax: %.*s", (int)token.length, token.start);
            return NULL;
        default:
            fail_at(parser, token, "expected expression; got %s", pc_token_name(token.type));
            return NULL;
    }
}

static pc_ast *parse_call(pc_parser *parser, pc_ast *callee, uint32_t line) {
    pc_ast *call = new_node(parser, AST_CALL, line);
    pc_ast *head = NULL;
    pc_ast *tail = NULL;
    size_t count = 0U;
    if (call == NULL) {
        return NULL;
    }
    while (!check(parser, T_RPAREN) && !check(parser, T_EOF) && !parser->failed) {
        pc_ast *argument;
        int spread = match(parser, T_ELLIPSIS);
        argument = parse_expression_precedence(parser, PREC_ASSIGN + 1);
        if (spread && argument != NULL) {
            pc_ast *spread_node = new_node(parser, AST_UNARY, argument->line);
            if (spread_node != NULL) {
                spread_node->as.unary.op = T_ELLIPSIS;
                spread_node->as.unary.operand = argument;
                argument = spread_node;
            }
        }
        pc_ast_append(&head, &tail, argument);
        count++;
        if (!match(parser, T_COMMA)) {
            break;
        }
    }
    (void)consume(parser, T_RPAREN, "expected ')' after arguments");
    call->as.call.callee = callee;
    call->as.call.arguments = head;
    call->as.call.count = count;
    return call;
}

static pc_ast *parse_infix(pc_parser *parser, pc_ast *left, pc_token op, int prec) {
    pc_ast *node;
    if (op.type == T_LPAREN) {
        return parse_call(parser, left, op.line);
    }
    if (op.type == T_LBRACKET) {
        node = new_node(parser, AST_INDEX, op.line);
        if (node != NULL) {
            node->as.index.base = left;
            node->as.index.key = check(parser, T_RBRACKET) ? NULL :
                                 parse_expression_precedence(parser, PREC_NONE + 1);
        }
        (void)consume(parser, T_RBRACKET, "expected ']' after index");
        return node;
    }
    if (op.type == T_ARROW || op.type == T_NULLSAFE_ARROW || op.type == T_SCOPE) {
        pc_token name = parser->current;
        if (name.type != T_IDENTIFIER && name.type != T_VARIABLE && name.type != T_CLASS) {
            fail_at(parser, name, "expected member name");
            return left;
        }
        advance_token(parser);
        node = new_node(parser, AST_MEMBER, op.line);
        if (node != NULL) {
            node->as.member.base = left;
            node->as.member.name = name;
            node->as.member.op = op.type;
        }
        return node;
    }
    if (op.type == T_PLUS_PLUS || op.type == T_MINUS_MINUS) {
        if (!is_lvalue(left)) {
            fail_at(parser, op, "increment target is not assignable");
        }
        node = new_node(parser, AST_UNARY, op.line);
        if (node != NULL) {
            node->as.unary.op = op.type;
            node->as.unary.operand = left;
            node->as.unary.postfix = 1;
        }
        return node;
    }
    if (op.type == T_QUESTION) {
        node = new_node(parser, AST_TERNARY, op.line);
        if (node != NULL) {
            node->as.ternary.condition = left;
            node->as.ternary.then_expr = check(parser, T_COLON) ? NULL :
                                         parse_expression_precedence(parser, PREC_NONE + 1);
            (void)consume(parser, T_COLON, "expected ':' in ternary expression");
            node->as.ternary.else_expr = parse_expression_precedence(parser, PREC_TERNARY);
        }
        return node;
    }
    node = new_node(parser, is_assignment(op.type) ? AST_ASSIGN : AST_BINARY, op.line);
    if (node == NULL) {
        return left;
    }
    if (is_assignment(op.type) && !is_lvalue(left)) {
        fail_at(parser, op, "left side of assignment is not assignable");
    }
    node->as.binary.op = op.type;
    node->as.binary.left = left;
    node->as.binary.right = parse_expression_precedence(
        parser, (is_assignment(op.type) || op.type == T_COALESCE || op.type == T_POW)
                    ? prec
                    : prec + 1);
    return node;
}

static pc_ast *parse_expression_precedence(pc_parser *parser, int minimum) {
    pc_ast *left;
    if (parser->failed) {
        return NULL;
    }
    left = parse_prefix(parser);
    while (!parser->failed) {
        int prec = precedence(parser->current.type);
        pc_token op;
        if (prec < minimum || prec == PREC_NONE) {
            break;
        }
        op = parser->current;
        advance_token(parser);
        left = parse_infix(parser, left, op, prec);
    }
    return left;
}

static pc_ast *parse_expression(pc_parser *parser) {
    return parse_expression_precedence(parser, PREC_NONE + 1);
}

static void consume_statement_end(pc_parser *parser) {
    if (match(parser, T_SEMICOLON)) {
        return;
    }
    if (check(parser, T_CLOSE_TAG) || check(parser, T_EOF) || check(parser, T_RBRACE)) {
        return;
    }
    fail_at(parser, parser->current, "expected ';' after statement");
}

static pc_ast *parse_statement(pc_parser *parser);
static pc_ast *parse_binding_statement(pc_parser *parser, pc_token keyword,
                                       pc_ast_kind kind);

static pc_ast *parse_block(pc_parser *parser, uint32_t line) {
    pc_ast *block = new_node(parser, AST_BLOCK, line);
    pc_ast *head = NULL;
    pc_ast *tail = NULL;
    size_t count = 0U;
    if (!enter_depth(parser)) {
        return block;
    }
    while (!check(parser, T_RBRACE) && !check(parser, T_EOF) && !parser->failed) {
        pc_ast *statement = parse_statement(parser);
        pc_ast_append(&head, &tail, statement);
        count++;
    }
    (void)consume(parser, T_RBRACE, "expected '}' after block");
    leave_depth(parser);
    if (block != NULL) {
        block->as.list.items = head;
        block->as.list.count = count;
    }
    return block;
}

static pc_ast *parse_condition(pc_parser *parser, const char *owner) {
    char message[96];
    (void)snprintf(message, sizeof(message), "expected '(' after %s", owner);
    (void)consume(parser, T_LPAREN, message);
    {
        pc_ast *condition = parse_expression(parser);
        (void)consume(parser, T_RPAREN, "expected ')' after condition");
        return condition;
    }
}

static pc_ast *parse_if(pc_parser *parser, pc_token keyword) {
    pc_ast *node = new_node(parser, AST_IF, keyword.line);
    pc_ast *tail = node;
    if (node == NULL) {
        return NULL;
    }
    node->as.if_stmt.condition = parse_condition(parser, "if");
    node->as.if_stmt.then_branch = parse_statement(parser);
    while (match(parser, T_ELSEIF)) {
        pc_ast *branch = new_node(parser, AST_IF, parser->previous.line);
        if (branch == NULL) {
            break;
        }
        branch->as.if_stmt.condition = parse_condition(parser, "elseif");
        branch->as.if_stmt.then_branch = parse_statement(parser);
        tail->as.if_stmt.else_branch = branch;
        tail = branch;
    }
    if (match(parser, T_ELSE)) {
        tail->as.if_stmt.else_branch = parse_statement(parser);
    }
    return node;
}

static pc_ast *parse_expression_list(pc_parser *parser, pc_token_type terminator) {
    pc_ast *head = NULL;
    pc_ast *tail = NULL;
    if (check(parser, terminator)) {
        return NULL;
    }
    do {
        pc_ast *expression = parse_expression(parser);
        pc_ast_append(&head, &tail, expression);
    } while (match(parser, T_COMMA));
    return head;
}

static pc_ast *parse_for(pc_parser *parser, pc_token keyword) {
    pc_ast *node = new_node(parser, AST_FOR, keyword.line);
    (void)consume(parser, T_LPAREN, "expected '(' after for");
    if (node != NULL) {
        node->as.for_stmt.initializers = parse_expression_list(parser, T_SEMICOLON);
    }
    (void)consume(parser, T_SEMICOLON, "expected ';' after for initializer");
    if (node != NULL) {
        node->as.for_stmt.conditions = parse_expression_list(parser, T_SEMICOLON);
    }
    (void)consume(parser, T_SEMICOLON, "expected ';' after for condition");
    if (node != NULL) {
        node->as.for_stmt.increments = parse_expression_list(parser, T_RPAREN);
    }
    (void)consume(parser, T_RPAREN, "expected ')' after for clauses");
    if (node != NULL) {
        node->as.for_stmt.body = parse_statement(parser);
    }
    return node;
}

static pc_ast *parse_foreach(pc_parser *parser, pc_token keyword) {
    pc_ast *node = new_node(parser, AST_FOREACH, keyword.line);
    pc_ast *first;
    (void)consume(parser, T_LPAREN, "expected '(' after foreach");
    if (node != NULL) {
        node->as.foreach_stmt.iterable = parse_expression(parser);
    }
    (void)consume(parser, T_AS, "expected 'as' in foreach");
    first = parse_expression_precedence(parser, PREC_ASSIGN + 1);
    if (match(parser, T_DOUBLE_ARROW)) {
        if (node != NULL) {
            node->as.foreach_stmt.key = first;
            node->as.foreach_stmt.value = parse_expression_precedence(parser, PREC_ASSIGN + 1);
        }
    } else if (node != NULL) {
        node->as.foreach_stmt.value = first;
    }
    if (node != NULL && (!is_lvalue(node->as.foreach_stmt.value) ||
                         (node->as.foreach_stmt.key != NULL &&
                          !is_lvalue(node->as.foreach_stmt.key)))) {
        fail_at(parser, keyword, "foreach key and value must be assignable");
    }
    (void)consume(parser, T_RPAREN, "expected ')' after foreach");
    if (node != NULL) {
        node->as.foreach_stmt.body = parse_statement(parser);
    }
    return node;
}

static pc_ast *parse_switch(pc_parser *parser, pc_token keyword) {
    pc_ast *node = new_node(parser, AST_SWITCH, keyword.line);
    pc_ast *cases = NULL;
    pc_ast *case_tail = NULL;
    int saw_default = 0;
    (void)consume(parser, T_LPAREN, "expected '(' after switch");
    if (node != NULL) node->as.switch_stmt.subject = parse_expression(parser);
    (void)consume(parser, T_RPAREN, "expected ')' after switch expression");
    (void)consume(parser, T_LBRACE, "expected '{' after switch expression");
    if (!enter_depth(parser)) return node;
    while (!check(parser, T_RBRACE) && !check(parser, T_EOF) && !parser->failed) {
        pc_ast *case_node;
        pc_ast *body;
        pc_ast *statements = NULL;
        pc_ast *statement_tail = NULL;
        size_t statement_count = 0U;
        pc_token label = parser->current;
        if (match(parser, T_CASE)) {
            case_node = new_node(parser, AST_CASE, label.line);
            if (case_node != NULL) {
                case_node->as.case_stmt.condition = parse_expression(parser);
            }
        } else if (match(parser, T_DEFAULT)) {
            case_node = new_node(parser, AST_CASE, label.line);
            if (saw_default) {
                fail_at(parser, label, "switch may only contain one default clause");
            }
            saw_default = 1;
        } else {
            fail_at(parser, parser->current,
                    "expected case or default in switch body");
            break;
        }
        (void)consume(parser, T_COLON, "expected ':' after switch label");
        while (!check(parser, T_CASE) && !check(parser, T_DEFAULT) &&
               !check(parser, T_RBRACE) && !check(parser, T_EOF) &&
               !parser->failed) {
            pc_ast *statement = parse_statement(parser);
            pc_ast_append(&statements, &statement_tail, statement);
            if (statement != NULL) statement_count++;
        }
        body = new_node(parser, AST_BLOCK, label.line);
        if (body != NULL) {
            body->as.list.items = statements;
            body->as.list.count = statement_count;
        }
        if (case_node != NULL) {
            case_node->as.case_stmt.body = body;
            pc_ast_append(&cases, &case_tail, case_node);
        }
    }
    (void)consume(parser, T_RBRACE, "expected '}' after switch body");
    leave_depth(parser);
    if (node != NULL) node->as.switch_stmt.cases = cases;
    return node;
}

static pc_ast *parse_echo(pc_parser *parser, pc_token keyword) {
    pc_ast *node = new_node(parser, AST_ECHO, keyword.line);
    pc_ast *head = NULL;
    pc_ast *tail = NULL;
    size_t count = 0U;
    do {
        pc_ast *expression = parse_expression_precedence(parser, PREC_ASSIGN + 1);
        pc_ast_append(&head, &tail, expression);
        count++;
    } while (match(parser, T_COMMA));
    consume_statement_end(parser);
    if (node != NULL) {
        node->as.list.items = head;
        node->as.list.count = count;
    }
    return node;
}

static int is_type_token(pc_token_type type) {
    return type == T_INT_TYPE || type == T_FLOAT_TYPE || type == T_STRING_TYPE ||
           type == T_BOOL_TYPE || type == T_ARRAY || type == T_CALLABLE ||
           type == T_MIXED || type == T_VOID || type == T_NULL ||
           type == T_SELF || type == T_STATIC || type == T_IDENTIFIER;
}

static void parse_optional_type(pc_parser *parser) {
    (void)match(parser, T_QUESTION);
    if (!is_type_token(parser->current.type)) {
        return;
    }
    advance_token(parser);
    while (match(parser, T_PIPE)) {
        if (!is_type_token(parser->current.type)) {
            fail_at(parser, parser->current, "expected type after '|'");
            return;
        }
        advance_token(parser);
    }
}

static pc_ast *parse_function(pc_parser *parser, pc_token keyword) {
    pc_ast *function = new_node(parser, AST_FUNCTION, keyword.line);
    pc_ast *head = NULL;
    pc_ast *tail = NULL;
    size_t count = 0U;
    pc_token name = consume(parser, T_IDENTIFIER, "expected function name");
    (void)consume(parser, T_LPAREN, "expected '(' after function name");
    while (!check(parser, T_RPAREN) && !check(parser, T_EOF) && !parser->failed) {
        pc_ast *parameter;
        int variadic;
        uint8_t parameter_flags = 0U;
        while (check(parser, T_PUBLIC) || check(parser, T_PROTECTED) ||
               check(parser, T_PRIVATE) || check(parser, T_READONLY)) {
            if (check(parser, T_PUBLIC)) parameter_flags |= PC_MOD_PUBLIC;
            else if (check(parser, T_PROTECTED)) parameter_flags |= PC_MOD_PROTECTED;
            else if (check(parser, T_PRIVATE)) parameter_flags |= PC_MOD_PRIVATE;
            else parameter_flags |= PC_MOD_READONLY;
            advance_token(parser);
        }
        if ((parameter_flags & PC_MOD_READONLY) != 0U &&
            (parameter_flags & (PC_MOD_PUBLIC | PC_MOD_PROTECTED |
                                PC_MOD_PRIVATE)) == 0U) {
            parameter_flags |= PC_MOD_PUBLIC;
        }
        if (!check(parser, T_VARIABLE) && !check(parser, T_ELLIPSIS)) {
            parse_optional_type(parser);
        }
        variadic = match(parser, T_ELLIPSIS);
        parameter = new_node(parser, AST_PARAM, parser->current.line);
        if (parameter != NULL) {
            parameter->as.parameter.name = consume(parser, T_VARIABLE,
                                                    "expected parameter variable");
            parameter->as.parameter.variadic = variadic;
            parameter->as.parameter.flags = parameter_flags;
            if (match(parser, T_EQUAL)) {
                parameter->as.parameter.default_value = parse_expression_precedence(
                    parser, PREC_ASSIGN + 1);
            }
        }
        pc_ast_append(&head, &tail, parameter);
        count++;
        if (!match(parser, T_COMMA)) {
            break;
        }
    }
    (void)consume(parser, T_RPAREN, "expected ')' after parameters");
    if (match(parser, T_COLON)) {
        parse_optional_type(parser);
    }
    if (match(parser, T_SEMICOLON)) {
        if (function != NULL) {
            function->as.function.body = new_node(parser, AST_BLOCK,
                                                  keyword.line);
            function->as.function.declaration_only = 1;
        }
    } else {
        (void)consume(parser, T_LBRACE, "expected function body");
        if (function != NULL) {
            function->as.function.body = parse_block(parser, keyword.line);
        }
    }
    if (function != NULL) {
        function->as.function.name = name;
        function->as.function.parameters = head;
        function->as.function.parameter_count = count;
        function->as.function.flags = PC_MOD_PUBLIC;
    }
    return function;
}

static uint8_t parse_member_modifiers(pc_parser *parser) {
    uint8_t flags = 0U;
    int scanning = 1;
    while (scanning) {
        switch (parser->current.type) {
            case T_PUBLIC: flags |= PC_MOD_PUBLIC; advance_token(parser); break;
            case T_PROTECTED: flags |= PC_MOD_PROTECTED; advance_token(parser); break;
            case T_PRIVATE: flags |= PC_MOD_PRIVATE; advance_token(parser); break;
            case T_STATIC: flags |= PC_MOD_STATIC; advance_token(parser); break;
            case T_ABSTRACT: flags |= PC_MOD_ABSTRACT; advance_token(parser); break;
            case T_FINAL: flags |= PC_MOD_FINAL; advance_token(parser); break;
            case T_READONLY: flags |= PC_MOD_READONLY; advance_token(parser); break;
            default: scanning = 0; break;
        }
    }
    if ((flags & (PC_MOD_PUBLIC | PC_MOD_PROTECTED | PC_MOD_PRIVATE)) == 0U) {
        flags |= PC_MOD_PUBLIC;
    }
    return flags;
}

static pc_ast *parse_class(pc_parser *parser, pc_token keyword, uint8_t flags) {
    pc_ast *class_node = new_node(parser, AST_CLASS, keyword.line);
    pc_ast *head = NULL;
    pc_ast *tail = NULL;
    pc_ast *interfaces = NULL;
    pc_ast *interface_tail = NULL;
    pc_token name = consume(parser, T_IDENTIFIER, "expected class name");
    pc_token parent;
    memset(&parent, 0, sizeof(parent));
    if (match(parser, T_EXTENDS)) {
        if ((flags & PC_MOD_INTERFACE) != 0U) {
            do {
                pc_token interface_name = consume(
                    parser, T_IDENTIFIER, "expected parent interface name");
                pc_ast_append(&interfaces, &interface_tail,
                              literal_node(parser, AST_IDENTIFIER,
                                           interface_name));
            } while (match(parser, T_COMMA));
        } else {
            parent = consume(parser, T_IDENTIFIER,
                             "expected parent class name");
        }
    }
    if (match(parser, T_IMPLEMENTS)) {
        do {
            pc_token interface_name = consume(
                parser, T_IDENTIFIER, "expected interface name");
            pc_ast_append(&interfaces, &interface_tail,
                          literal_node(parser, AST_IDENTIFIER,
                                       interface_name));
        } while (match(parser, T_COMMA));
    }
    (void)consume(parser, T_LBRACE, "expected '{' after class declaration");
    while (!check(parser, T_RBRACE) && !check(parser, T_EOF) && !parser->failed) {
        uint8_t member_flags = parse_member_modifiers(parser);
        if (match(parser, T_FUNCTION)) {
            pc_ast *method = parse_function(parser, parser->previous);
            if (method != NULL) {
                if ((flags & PC_MOD_INTERFACE) != 0U) {
                    if ((member_flags & (PC_MOD_PRIVATE | PC_MOD_PROTECTED |
                                         PC_MOD_FINAL)) != 0U) {
                        fail_at(parser, method->as.function.name,
                                "interface methods must be public and non-final");
                    }
                    member_flags &= (uint8_t)~(PC_MOD_PRIVATE |
                                               PC_MOD_PROTECTED);
                    member_flags |= PC_MOD_PUBLIC | PC_MOD_ABSTRACT;
                }
                method->as.function.flags = member_flags;
                pc_ast_append(&head, &tail, method);
            }
            continue;
        }
        if (match(parser, T_CONST)) {
            pc_ast *constant = parse_binding_statement(parser, parser->previous, AST_CONST);
            while (constant != NULL) {
                pc_ast *next = constant->next;
                pc_ast_append(&head, &tail, constant);
                constant = next;
            }
            continue;
        }
        if (!check(parser, T_VARIABLE)) {
            parse_optional_type(parser);
        }
        do {
            pc_ast *property = new_node(parser, AST_PROPERTY, parser->current.line);
            if (property != NULL) {
                property->as.property.name = consume(parser, T_VARIABLE,
                                                      "expected property name");
                property->as.property.flags = member_flags;
                if (match(parser, T_EQUAL)) {
                    property->as.property.default_value =
                        parse_expression_precedence(parser, PREC_ASSIGN + 1);
                }
                pc_ast_append(&head, &tail, property);
            }
        } while (match(parser, T_COMMA));
        consume_statement_end(parser);
    }
    (void)consume(parser, T_RBRACE, "expected '}' after class body");
    if (class_node != NULL) {
        class_node->as.class_decl.name = name;
        class_node->as.class_decl.parent = parent;
        class_node->as.class_decl.interfaces = interfaces;
        class_node->as.class_decl.members = head;
        class_node->as.class_decl.flags = flags;
    }
    return class_node;
}

static pc_ast *parse_try_statement(pc_parser *parser, pc_token keyword) {
    pc_ast *node = new_node(parser, AST_TRY, keyword.line);
    pc_ast *catches = NULL;
    pc_ast *tail = NULL;
    (void)consume(parser, T_LBRACE, "expected '{' after try");
    if (node != NULL) {
        node->as.try_stmt.try_block = parse_block(parser, keyword.line);
    }
    while (match(parser, T_CATCH)) {
        pc_token catch_keyword = parser->previous;
        pc_ast *catch_node = new_node(parser, AST_CATCH, catch_keyword.line);
        pc_ast *types = NULL;
        pc_ast *type_tail = NULL;
        pc_token variable;
        memset(&variable, 0, sizeof(variable));
        (void)consume(parser, T_LPAREN, "expected '(' after catch");
        do {
            pc_token type = consume(parser, T_IDENTIFIER,
                                    "expected exception class in catch");
            pc_ast_append(&types, &type_tail,
                          literal_node(parser, AST_IDENTIFIER, type));
        } while (match(parser, T_PIPE));
        if (check(parser, T_VARIABLE)) {
            variable = parser->current;
            advance_token(parser);
        }
        (void)consume(parser, T_RPAREN, "expected ')' after catch declaration");
        (void)consume(parser, T_LBRACE, "expected catch body");
        if (catch_node != NULL) {
            catch_node->as.catch_stmt.types = types;
            catch_node->as.catch_stmt.variable = variable;
            catch_node->as.catch_stmt.body = parse_block(parser, catch_keyword.line);
            pc_ast_append(&catches, &tail, catch_node);
        }
    }
    if (match(parser, T_FINALLY)) {
        pc_token finally_keyword = parser->previous;
        (void)consume(parser, T_LBRACE, "expected finally body");
        if (node != NULL) {
            node->as.try_stmt.finally_block = parse_block(parser, finally_keyword.line);
        }
    }
    if (node != NULL) {
        node->as.try_stmt.catches = catches;
        if (catches == NULL && node->as.try_stmt.finally_block == NULL) {
            fail_at(parser, keyword, "try requires catch or finally");
        }
    }
    return node;
}

static pc_ast *parse_binding_statement(pc_parser *parser, pc_token keyword,
                                       pc_ast_kind kind) {
    pc_ast *head = NULL;
    pc_ast *tail = NULL;
    do {
        pc_ast *binding = new_node(parser, kind, parser->current.line);
        pc_token_type required = kind == AST_CONST ? T_IDENTIFIER : T_VARIABLE;
        if (binding != NULL) {
            binding->as.binding.name = consume(parser, required, "expected binding name");
            if (match(parser, T_EQUAL)) {
                binding->as.binding.value = parse_expression_precedence(parser, PREC_ASSIGN + 1);
            } else if (kind == AST_CONST) {
                fail_at(parser, parser->current, "constant requires an initializer");
            }
        }
        pc_ast_append(&head, &tail, binding);
    } while (match(parser, T_COMMA));
    consume_statement_end(parser);
    if (head != NULL) {
        head->line = keyword.line;
    }
    return head;
}

static pc_ast *parse_statement(pc_parser *parser) {
    pc_token token = parser->current;
    pc_ast *node;
    if (match(parser, T_SEMICOLON) || match(parser, T_OPEN_TAG) ||
        match(parser, T_CLOSE_TAG)) {
        return NULL;
    }
    if (match(parser, T_INLINE_HTML)) {
        node = literal_node(parser, AST_STRING, token);
        {
            pc_ast *echo = new_node(parser, AST_ECHO, token.line);
            if (echo != NULL) {
                echo->as.list.items = node;
                echo->as.list.count = 1U;
            }
            return echo;
        }
    }
    if (match(parser, T_OPEN_TAG_ECHO)) {
        return parse_echo(parser, token);
    }
    if (match(parser, T_LBRACE)) {
        return parse_block(parser, token.line);
    }
    if (match(parser, T_ECHO)) {
        return parse_echo(parser, token);
    }
    if (match(parser, T_IF)) {
        return parse_if(parser, token);
    }
    if (match(parser, T_WHILE)) {
        node = new_node(parser, AST_WHILE, token.line);
        if (node != NULL) {
            node->as.loop.condition = parse_condition(parser, "while");
            node->as.loop.body = parse_statement(parser);
        }
        return node;
    }
    if (match(parser, T_DO)) {
        node = new_node(parser, AST_DO_WHILE, token.line);
        if (node != NULL) {
            node->as.loop.body = parse_statement(parser);
        }
        (void)consume(parser, T_WHILE, "expected while after do body");
        if (node != NULL) {
            node->as.loop.condition = parse_condition(parser, "while");
        }
        consume_statement_end(parser);
        return node;
    }
    if (match(parser, T_FOR)) {
        return parse_for(parser, token);
    }
    if (match(parser, T_FOREACH)) {
        return parse_foreach(parser, token);
    }
    if (match(parser, T_SWITCH)) {
        return parse_switch(parser, token);
    }
    if (match(parser, T_BREAK) || match(parser, T_CONTINUE)) {
        pc_ast_kind kind = token.type == T_BREAK ? AST_BREAK : AST_CONTINUE;
        node = new_node(parser, kind, token.line);
        if (node != NULL) {
            node->as.jump.level = 1U;
            if (check(parser, T_INTEGER)) {
                unsigned value = 0U;
                size_t i;
                for (i = 0U; i < parser->current.length; i++) {
                    char digit = parser->current.start[i];
                    if (digit != '_') {
                        value = value * 10U + (unsigned)(digit - '0');
                    }
                }
                node->as.jump.level = value;
                advance_token(parser);
            }
            if (node->as.jump.level == 0U) {
                fail_at(parser, token, "break/continue level must be at least 1");
            }
        }
        consume_statement_end(parser);
        return node;
    }
    if (match(parser, T_RETURN) || match(parser, T_THROW)) {
        pc_ast_kind kind = token.type == T_RETURN ? AST_RETURN : AST_THROW;
        node = new_node(parser, kind, token.line);
        if (node != NULL && !check(parser, T_SEMICOLON) && !check(parser, T_CLOSE_TAG) &&
            !check(parser, T_RBRACE)) {
            node->as.expression.expression = parse_expression(parser);
        }
        if (kind == AST_THROW && node != NULL && node->as.expression.expression == NULL) {
            fail_at(parser, token, "throw requires an expression");
        }
        consume_statement_end(parser);
        return node;
    }
    if (check(parser, T_FUNCTION)) {
        advance_token(parser);
        if (check(parser, T_IDENTIFIER)) {
            return parse_function(parser, token);
        }
        node = new_node(parser, AST_EXPR_STMT, token.line);
        if (node != NULL) {
            node->as.expression.expression = parse_closure(parser, token, 0, 0);
        }
        consume_statement_end(parser);
        return node;
    }
    if (match(parser, T_TRY)) {
        return parse_try_statement(parser, token);
    }
    if (match(parser, T_CLASS)) {
        return parse_class(parser, token, 0U);
    }
    if (match(parser, T_INTERFACE)) {
        return parse_class(parser, token, PC_MOD_INTERFACE | PC_MOD_ABSTRACT);
    }
    if (match(parser, T_FINAL) || match(parser, T_ABSTRACT) || match(parser, T_READONLY)) {
        uint8_t flags = token.type == T_FINAL ? PC_MOD_FINAL :
                        (token.type == T_ABSTRACT ? PC_MOD_ABSTRACT : PC_MOD_READONLY);
        pc_token class_keyword = consume(parser, T_CLASS, "expected class after modifier");
        return parse_class(parser, class_keyword, flags);
    }
    if (match(parser, T_GLOBAL)) {
        pc_ast *global = new_node(parser, AST_GLOBAL, token.line);
        pc_ast *head = NULL;
        pc_ast *tail = NULL;
        size_t count = 0U;
        do {
            pc_token variable = consume(parser, T_VARIABLE, "expected global variable");
            pc_ast_append(&head, &tail, literal_node(parser, AST_VARIABLE, variable));
            count++;
        } while (match(parser, T_COMMA));
        consume_statement_end(parser);
        if (global != NULL) {
            global->as.list.items = head;
            global->as.list.count = count;
        }
        return global;
    }
    if (check(parser, T_STATIC)) {
        advance_token(parser);
        if (check(parser, T_FUNCTION) || check(parser, T_FN)) {
            pc_token closure_keyword = parser->current;
            int is_arrow = closure_keyword.type == T_FN;
            advance_token(parser);
            node = new_node(parser, AST_EXPR_STMT, token.line);
            if (node != NULL) {
                node->as.expression.expression = parse_closure(
                    parser, closure_keyword, is_arrow, 1);
            }
            consume_statement_end(parser);
            return node;
        }
        return parse_binding_statement(parser, token, AST_STATIC);
    }
    if (match(parser, T_CONST)) {
        return parse_binding_statement(parser, token, AST_CONST);
    }
    if (match(parser, T_INCLUDE) || match(parser, T_INCLUDE_ONCE) ||
        match(parser, T_REQUIRE) || match(parser, T_REQUIRE_ONCE)) {
        node = new_node(parser, AST_INCLUDE, token.line);
        if (node != NULL) {
            node->as.include_stmt.mode = token.type;
            node->as.include_stmt.path = parse_expression(parser);
        }
        consume_statement_end(parser);
        return node;
    }
    if (check(parser, T_NEW)) {
        fail_at(parser, parser->current, "syntax %.*s is reserved for a later runtime milestone",
                (int)parser->current.length, parser->current.start);
        return NULL;
    }
    node = new_node(parser, AST_EXPR_STMT, token.line);
    if (node != NULL) {
        node->as.expression.expression = parse_expression(parser);
    }
    consume_statement_end(parser);
    return node;
}

void pc_parser_init(pc_parser *parser, pc_arena *arena, const char *source,
                    size_t length, int repl) {
    memset(parser, 0, sizeof(*parser));
    parser->arena = arena;
    pc_lexer_init(&parser->lexer, source, length, repl);
    advance_token(parser);
}

pc_ast *pc_parse_program(pc_parser *parser) {
    pc_ast *program = new_node(parser, AST_PROGRAM, 1U);
    pc_ast *head = NULL;
    pc_ast *tail = NULL;
    size_t count = 0U;
    while (!check(parser, T_EOF) && !parser->failed) {
        pc_ast *statement = parse_statement(parser);
        if (statement != NULL) {
            pc_ast_append(&head, &tail, statement);
            count++;
        }
    }
    if (program != NULL) {
        program->as.list.items = head;
        program->as.list.count = count;
    }
    return parser->failed ? NULL : program;
}

const char *pc_parser_error(const pc_parser *parser) {
    return parser->error;
}

uint32_t pc_parser_error_line(const pc_parser *parser) {
    return parser->previous.line != 0U ? parser->previous.line : parser->current.line;
}
