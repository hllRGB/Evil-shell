/* vim: set ft=c ts=8 sw=8 sts=8 et: */
#ifndef SH_OPERATION_H
#define SH_OPERATION_H

#include "include/evilgeneral.h"
#include "libs/memm.h"               /* STALLOC_T */

/* ── 前向声明 ── */

typedef struct we_s WE_T;            /* WORD_T 内 WE_T* */
typedef struct word_s WORD_T;        /* WE_T 内嵌 by-value */
typedef struct command_str COMMAND_T;/* WE_CMDSUBST */

/* ── 枚举 ── */

typedef enum we_type {
    WE_LITERAL, WE_VAR, WE_CMDSUBST, WE_ARITH, WE_TILDE,
} WE_TYPE;

typedef enum var_subop {
    V_NONE, V_INDIRECT, V_LEN, V_CASE,
    V_DEFAULT, V_ASSIGN, V_ERROR, V_ALT,
    V_PREFIX, V_SUFFIX, V_PATSUBST, V_SUBSTR,
} VAR_SUBOP;

typedef enum { WE_F_QUOTED = 0x01, WE_F_HAD_UNQUOTED = 0x02 } WE_FLAGS;

typedef enum cmd_type {
    CMD_FOR, CMD_CASE, CMD_WHILE, CMD_IF, CMD_SIMPLE,
    CMD_SELECT, CMD_CONNECTION, CMD_FUNCTION_DEF, CMD_UNTIL,
    CMD_GROUP, CMD_ARITH, CMD_COND, CMD_ARITH_FOR,
    CMD_SUBSHELL, CMD_COPROC,
} CMD_TYPE;

enum redir_instruction {
    REDIR_OUT, REDIR_IN, REDIR_INA, REDIR_APPEND,
    REDIR_READ_UNTIL, REDIR_READ_STRING, REDIR_DUP_IN,
    REDIR_DUP_OUT, REDIR_DEBLANK_READ, REDIR_CLOSE,
    REDIR_ERR_OUT, REDIR_IN_OUT, REDIR_OUT_FORCE,
    REDIR_DUP_IN_WORD, REDIR_DUP_OUT_WORD,
    REDIR_MV_IN, REDIR_MV_OUT, REDIR_MV_IN_WORD, REDIR_MV_OUT_WORD,
    REDIR_APPEND_ERR_OUT,
};

typedef enum { CASEPAT_FALLTHROUGH = 1 << 0, CASEPAT_TESTNEXT = 1 << 1 } CASEPAT_FLAGS;

typedef enum { CONN_AND_AND = 1, CONN_OR_OR, CONN_SEMI, CONN_AMP,
               CONN_PIPE, CONN_BAR_AND } CONN_TYPE;

typedef enum {
    CMD_WANT_SUBSHELL   = 1 << 0,
    CMD_INVERT_RETURN   = 1 << 1,
    CMD_AMPERSAND       = 1 << 2,
    CMD_TIME_PIPELINE   = 1 << 3,
    CMD_COPROC_SUBSHELL = 1 << 4,
    CMD_LASTPIPE        = 1 << 5,
} CMD_FLAGS;

typedef enum {
    COND_AND = 1, COND_OR, COND_UNARY, COND_BINARY, COND_TERM, COND_EXPR,
} COND_TYPE;

/* ── 基础组件 ── */

/* WORD_T: WE_T 链表头 */
struct word_s {
    WE_T *nullable elements;
    uint32_t flags;
};

/* WE_T: 构件码链表节点 */
struct we_s {
    WE_TYPE type;
    VAR_SUBOP sub_op;                /* 仅 WE_VAR 有意义 */
    WE_FLAGS flags;
    WE_T *nullable next;
    union {
        char *nonnull literal;                     /* WE_LITERAL */
        struct { WORD_T name; int case_op; } var_simple;
        struct { WORD_T name; WE_T *nullable word; } var_word;
        struct { WORD_T name; WE_T *nullable a; WE_T *nullable b; } var_pair;
        COMMAND_T *nullable cmd;                   /* WE_CMDSUBST */
        WE_T *nullable arith;                      /* WE_ARITH */
        char *nullable prefix;                     /* WE_TILDE */
    } data;
};

/* WORD_LIST_T: WORD_T 链表 */
typedef struct word_list_s {
    struct word_list_s *nullable next;
    WORD_T word;
} WORD_LIST_T;

/* ── 重定向 ── */

typedef union {
    int dest;
    WORD_T *nullable path;
} REDIRECTEE_T;

typedef struct redir_s {
    struct redir_s *nullable next;
    REDIRECTEE_T redirector;
    uint32_t rflags;
    int openflags;
    enum redir_instruction instruction;
    REDIRECTEE_T redirectee;
    char *nullable heredoc_eof;
} REDIR_T;

/* ── case 匹配 ── */

typedef struct pattern_list_s {
    struct pattern_list_s *nullable next;
    WORD_LIST_T *nullable patterns;
    COMMAND_T *nullable action;
    CASEPAT_FLAGS flags;
} PATTERN_LIST_T;

/* ── parser 临时积累 ── */

typedef struct element_s {
    WORD_T *nullable word;
    REDIR_T *nullable redirect;
} ELEMENT_T;

/* ── 命令子结构 ── */

struct for_cmd {
    char *nonnull var_name;
    WORD_LIST_T *nullable map_list;  /* NULL = "$@" */
    COMMAND_T *nonnull body;
};

struct case_cmd {
    WORD_T *nonnull word;
    PATTERN_LIST_T *nullable clauses;
};

struct if_cmd {
    COMMAND_T *nonnull condition;
    COMMAND_T *nonnull then_branch;
    COMMAND_T *nullable else_branch;
};

struct while_cmd {
    COMMAND_T *nonnull condition;
    COMMAND_T *nonnull body;         /* UNTIL 共用，flags 区分 */
};

struct for_arith_cmd {
    WORD_LIST_T *nullable init;
    WORD_LIST_T *nullable test;
    WORD_LIST_T *nullable step;
    COMMAND_T *nonnull body;
};

struct select_cmd {
    char *nonnull var_name;
    WORD_LIST_T *nullable map_list;
    COMMAND_T *nonnull body;
};

struct function_def {
    char *nonnull name;
    COMMAND_T *nonnull body;
    char *nullable source_file;
};

struct group_cmd {
    COMMAND_T *nonnull body;
};

struct subshell_cmd {
    COMMAND_T *nonnull body;
};

struct arith_cmd {
    WORD_LIST_T *nonnull exp;        /* ((...)) 表达式 */
};

struct cond_cmd {
    COND_TYPE cond_type;
    WORD_T *nullable op;             /* TERM 时 NULL */
    struct cond_cmd *nullable left;
    struct cond_cmd *nullable right;
};

struct coproc_cmd {
    char *nullable name;             /* NULL = 匿名 */
    COMMAND_T *nonnull body;
};

/* ── COMMAND_T 主干 ── */

struct command_str {
    CMD_TYPE type;
    CMD_FLAGS flags;
    uint32_t line;
    REDIR_T *nullable redirects;
    union {
        struct for_cmd *nonnull for_cmd;
        struct case_cmd *nonnull case_cmd;
        struct while_cmd *nonnull while_cmd;
        struct if_cmd *nonnull if_cmd;
        struct connection_cmd *nonnull connection;
        struct simple_cmd *nonnull simple;
        struct function_def *nonnull function;
        struct group_cmd *nonnull group;
        struct select_cmd *nonnull select;
        struct arith_cmd *nonnull arith;
        struct cond_cmd *nonnull cond;
        struct for_arith_cmd *nonnull for_arith;
        struct subshell_cmd *nonnull subshell;
        struct coproc_cmd *nonnull coproc;
    } body;
};

/* ── 组合 ── */

typedef struct connection_cmd {
    COMMAND_T *nonnull left;
    COMMAND_T *nonnull right;
    CONN_TYPE connector;
} CONNECTION_CMD_T;

/* ── 简单命令 ── */

typedef struct simple_cmd {
    WORD_LIST_T *nonnull words;
} SIMPLE_CMD_T;

#endif /* SH_OPERATION_H */
