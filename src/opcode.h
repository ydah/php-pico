#ifndef PPHP_OPCODE_H
#define PPHP_OPCODE_H

#include <stdint.h>

/*
 * Ownership convention:
 * - LOAD operations push an owned value (heap values are retained).
 * - STORE operations consume one owned value.
 * - Binary operations consume both operands and push one owned result.
 * - POP releases its operand; DUP retains the duplicate.
 * - CALL consumes its arguments and pushes one owned result.
 */
typedef enum pphp_opcode {
    OP_NOP = 0x00,
    OP_HALT = 0x01,
    OP_POP = 0x02,
    OP_DUP = 0x03,
    OP_SWAP = 0x04,
    OP_LOAD_NULL = 0x10,
    OP_LOAD_TRUE = 0x11,
    OP_LOAD_FALSE = 0x12,
    OP_LOAD_I8 = 0x13,
    OP_LOAD_I32 = 0x14,
    OP_LOAD_CONST = 0x15,
    OP_LOAD_LOCAL = 0x16,
    OP_STORE_LOCAL = 0x17,
    OP_ADD = 0x20,
    OP_SUB = 0x21,
    OP_MUL = 0x22,
    OP_DIV = 0x23,
    OP_MOD = 0x24,
    OP_POW = 0x25,
    OP_NEG = 0x26,
    OP_CONCAT = 0x27,
    OP_BAND = 0x28,
    OP_BOR = 0x29,
    OP_BXOR = 0x2a,
    OP_BNOT = 0x2b,
    OP_SHL = 0x2c,
    OP_SHR = 0x2d,
    OP_EQ = 0x30,
    OP_NE = 0x31,
    OP_IDENT = 0x32,
    OP_NIDENT = 0x33,
    OP_LT = 0x34,
    OP_LE = 0x35,
    OP_GT = 0x36,
    OP_GE = 0x37,
    OP_CMP = 0x38,
    OP_NOT = 0x39,
    OP_JMP = 0x40,
    OP_JMP_IF = 0x41,
    OP_JMP_UNLESS = 0x42,
    OP_JMP_IF_KEEP = 0x43,
    OP_JMP_UNLESS_KEEP = 0x44,
    OP_JMP_NOTNULL_KEEP = 0x45,
    OP_CALL = 0x50,
    OP_CALL_VALUE = 0x51,
    OP_RET = 0x52,
    OP_RET_NULL = 0x53,
    OP_ECHO = 0x54,
    OP_NEW_ARRAY = 0x60,
    OP_ARR_PUSH = 0x61,
    OP_ARR_SET = 0x62,
    OP_IDX_GET = 0x63,
    OP_IDX_SET = 0x64,
    OP_IDX_APPEND = 0x65,
    OP_FE_INIT = 0x66,
    OP_FE_NEXT = 0x67,
    OP_FE_FREE = 0x68,
    OP_NEW_OBJ = 0x70,
    OP_PROP_GET = 0x71,
    OP_PROP_SET = 0x72,
    OP_MCALL = 0x73,
    OP_INSTANCEOF = 0x78,
    OP_CLOSURE = 0x80,
    OP_THROW = 0x81,
    OP_CLONE = 0x82,
    OP_DEF_CLASS = 0x86,
    OP_DEF_METHOD = 0x87,
    OP_DEF_PROP = 0x88,
    OP_DEF_END = 0x8a,
    OP_LINE = 0x90
} pphp_opcode;

const char *pphp_opcode_name(uint8_t opcode);

#endif
