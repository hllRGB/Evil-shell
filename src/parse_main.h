/* vim: set ft=c ts=8 sw=8 sts=8 et: */
#ifndef SH_PARSE_MAIN_H
#define SH_PARSE_MAIN_H

#include "include/evilgeneral.h"
#include "libs/memm.h"
#include "operation.h"

/* ── 输入游标 ── */

typedef struct input_s {
        const char * nonnull source;
        const char * nonnull cursor;
        const char * nonnull end;
} INPUT;

/* ── Token 类型 ── */

typedef enum tok_type {
        /* keywords */
        TOK_IF = 1,
        TOK_THEN,
        TOK_ELSE,
        TOK_ELIF,
        TOK_FI,
        TOK_CASE,
        TOK_ESAC,
        TOK_IN,
        TOK_FOR,
        TOK_SELECT,
        TOK_WHILE,
        TOK_UNTIL,
        TOK_DO,
        TOK_DONE,
        TOK_FUNCTION,
        TOK_COPROC,
        TOK_BANG,
        TOK_TIME,
        TOK_TIMEOPT,
        TOK_TIMEIGN,
        /* logical operators */
        TOK_AND_AND,
        TOK_OR_OR,
        TOK_PIPE,
        TOK_BAR_AND,
        /* list terminators */
        TOK_SEMI,
        TOK_AMP,
        TOK_NEWLINE,
        TOK_EOF,
        /* case separators */
        TOK_DSEMI,
        TOK_SEMI_AMP,
        TOK_DSEMI_AMP,
        /* redirection */
        TOK_REDIR,
        /* values */
        TOK_WORD,
        TOK_ASSIGNMENT_WORD,
        /* special compounds */
        TOK_COND_START,
        TOK_COND_END,
        TOK_ARITH_CMD,
        TOK_ARITH_FOR_EXPRS,
        /* grouping */
        TOK_LPAREN,
        TOK_RPAREN,
} TOK_TYPE;

typedef struct redir_token_s {
        int fd;
        int instruction; /* enum redir_instruction */
} REDIR_TOKEN_T;

typedef struct token_s {
        TOK_TYPE type;
        const char * nonnull start;
        const char * nonnull end;
        union {
                WORD_T word;         /* TOK_WORD / ASSIGNMENT_WORD */
                REDIR_TOKEN_T redir; /* TOK_REDIR */
        } data;
} TOKEN_T;

/* ── Lex flags ── */

#define CHKNL 1u    /* eat newlines */
#define CHKKWD 2u   /* promote to keyword */
#define CHKALIAS 4u /* expand aliases */

/* ── 函数原型 ── */

COMMAND_T * nullable parse_main(INPUT * nonnull in,
                                TOKEN_T * nonnull ctx,
                                STAREA_T * nonnull st);
TOKEN_T * nonnull lex(INPUT * nonnull in,
                      TOKEN_T * nonnull ctx,
                      uint8_t flags,
                      STAREA_T * nonnull st);
WORD_T * nullable parse_word(INPUT * nonnull in,
                             TOKEN_T * nonnull ctx,
                             STAREA_T * nonnull st);

#endif /* SH_PARSE_MAIN_H */
