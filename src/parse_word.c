/* vim: set ft=c ts=8 sw=8 sts=8 et: */
#include "include/evilgeneral.h"
#include "parse_word.h"
#include <string.h>

static bool
is_sep(char c)
{
    return c == ' ' || c == '\t' || c == '\n'
        || c == '|' || c == '&' || c == ';'
        || c == '(' || c == ')'
        || c == '<' || c == '>';
}

static WE_T *nonnull
we_alloc(STALLOC_T *nonnull st, WE_TYPE type)
{
    WE_T *we = stalloc(st, sizeof(WE_T));
    memset(we, 0, sizeof(WE_T));
    we->type = type;
    return we;
}

static WE_T *nonnull
we_literal(STALLOC_T *nonnull st, const char *nonnull start, size_t len)
{
    WE_T *we = we_alloc(st, WE_LITERAL);
    char *s = stalloc(st, len + 1);
    memcpy(s, start, len);
    s[len] = '\0';
    we->data.literal = s;
    return we;
}

static void
we_append(WE_T *nullable *nonnull tail, WE_T *nonnull we)
{
    we->next = NULL;
    *tail = we;
}

/* advance past a matching ')', handling nested $() */
static bool
skip_cmdsubst(INPUT *nonnull in)
{
    int depth = 1;
    while (in->cursor < in->end && depth > 0) {
        char c = *in->cursor++;
        if (c == '\\' && in->cursor < in->end) {
            in->cursor++;       /* skip escaped char */
            continue;
        }
        if (c == '\'') {
            while (in->cursor < in->end && *in->cursor != '\'') in->cursor++;
            if (in->cursor < in->end) in->cursor++;  /* skip ' */
            continue;
        }
        if (c == '"') {
            while (in->cursor < in->end) {
                char q = *in->cursor++;
                if (q == '\\' && in->cursor < in->end) { in->cursor++; continue; }
                if (q == '"') break;
            }
            continue;
        }
        if (c == '(') depth++;
        if (c == ')') depth--;
    }
    return depth == 0;
}

/* skip ${}, handling nested quotes/braces */
static bool
skip_param(INPUT *nonnull in)
{
    int depth = 1;
    bool dq = false;
    while (in->cursor < in->end && depth > 0) {
        char c = *in->cursor++;
        if (c == '\\' && in->cursor < in->end) { in->cursor++; continue; }
        if (c == '\'') { while (in->cursor < in->end && *in->cursor != '\'') in->cursor++; if (in->cursor < in->end) in->cursor++; continue; }
        if (c == '"') { dq = !dq; continue; }
        if (!dq) {
            if (c == '{') depth++;
            if (c == '}') depth--;
        }
    }
    return depth == 0;
}

/* read a NAME char: [a-zA-Z_][a-zA-Z0-9_]* */
static const char *nonnull
read_name(const char *nonnull start, const char *nonnull end)
{
    const char *p = start;
    if (p >= end) return p;
    if (*p != '_' && !((unsigned char)*p >= 'a' && (unsigned char)*p <= 'z')
        && !((unsigned char)*p >= 'A' && (unsigned char)*p <= 'Z'))
        return p;
    p++;
    while (p < end) {
        char c = *p;
        if (c == '_' || ((unsigned char)c >= 'a' && (unsigned char)c <= 'z')
            || ((unsigned char)c >= 'A' && (unsigned char)c <= 'Z')
            || ((unsigned char)c >= '0' && (unsigned char)c <= '9'))
            p++;
        else break;
    }
    return p;
}

WORD_T *nullable
parse_word(INPUT *nonnull in, TOKEN_T *nonnull ctx, STALLOC_T *nonnull st)
{
    (void)ctx;
    WORD_T *word = stalloc(st, sizeof(WORD_T));
    memset(word, 0, sizeof(WORD_T));
    WE_T *nullable *tail = &word->elements;
    bool in_quote = false;
    bool word_start = true;

    while (in->cursor < in->end) {
        char c = *in->cursor;

        /* unquoted separator → word end */
        if (!in_quote && is_sep(c))
            break;

        WE_T *we = NULL;

        switch (c) {
        case '\\': {
            in->cursor++;
            if (in->cursor < in->end) {
                we = we_literal(st, in->cursor, 1);
                in->cursor++;
            }
            break;
        }

        case '\'': {
            in->cursor++;
            if (!in_quote) {
                const char *s = in->cursor;
                while (in->cursor < in->end && *in->cursor != '\'') in->cursor++;
                we = we_literal(st, s, in->cursor - s);
                if (in->cursor < in->end) in->cursor++;  /* skip ' */
                we->flags |= WE_F_QUOTED;
            } else {
                we = we_literal(st, "'", 1);
            }
            break;
        }

        case '"': {
            in->cursor++;
            in_quote = !in_quote;
            /* mark HAD_UNQUOTED on toggle */
            continue;
        }

        case '$': {
            in->cursor++;
            if (in->cursor >= in->end) {
                we = we_literal(st, "$", 1);
                break;
            }
            switch (*in->cursor) {
            case '(': {
                in->cursor++;
                if (in->cursor < in->end && *in->cursor == '(') {
                    /* $(( arith )) */
                    in->cursor++;
                    const char *as = in->cursor;
                    /* skip to )) */
                    int d = 0;
                    while (in->cursor < in->end) {
                        char ac = *in->cursor++;
                        if (ac == '(') d++;
                        if (ac == ')') { if (d == 0) break; d--; }
                    }
                    if (in->cursor < in->end) in->cursor++;  /* skip 2nd ) */
                    we = we_alloc(st, WE_ARITH);
                    we->data.arith = NULL;  /* TODO: parse_arith */
                    (void)as;
                } else {
                    /* $( cmdsubst ) */
                    we = we_alloc(st, WE_CMDSUBST);
                    if (!skip_cmdsubst(in)) { /* error */ }
                    we->data.cmd = NULL;  /* TODO: recursive parse_main */
                }
                break;
            }
            case '{': {
                /* ${ param } */
                in->cursor++;
                we = we_alloc(st, WE_VAR);
                we->sub_op = V_NONE;
                /* read name */
                const char *ns = in->cursor;
                const char *ne = read_name(ns, in->end);
                WORD_T *wname = stalloc(st, sizeof(WORD_T));
                memset(wname, 0, sizeof(WORD_T));
                if (ne > ns) {
                    we->data.var_simple.name.elements = we_literal(st, ns, ne - ns);
                }
                in->cursor = ne;
                /* check for :? etc. — minimal for now */
                skip_param(in);
                break;
            }
            default: {
                /* $name — simple variable */
                const char *ns = in->cursor;
                const char *ne = read_name(ns, in->end);
                if (ne > ns || *in->cursor == '@' || *in->cursor == '*') {
                    if (ne == ns) ne = in->cursor + 1;  /* $@, $* */
                    we = we_alloc(st, WE_VAR);
                    we->sub_op = V_NONE;
                    WORD_T *wname = stalloc(st, sizeof(WORD_T));
                    memset(wname, 0, sizeof(WORD_T));
                    wname->elements = we_literal(st, ns, ne - ns);
                    we->data.var_simple.name = *wname;
                    in->cursor = ne;
                } else {
                    we = we_literal(st, "$", 1);
                }
                break;
            }
            }
            break;
        }

        case '~': {
            if (word_start && !in_quote) {
                in->cursor++;
                we = we_alloc(st, WE_TILDE);
                we->data.prefix = NULL;  /* default = $HOME */
            } else {
                in->cursor++;
                we = we_literal(st, "~", 1);
            }
            break;
        }

        default: {
            const char *s = in->cursor;
            while (in->cursor < in->end && !is_sep(*in->cursor)
                   && *in->cursor != '\\' && *in->cursor != '\''
                   && *in->cursor != '"' && *in->cursor != '$'
                   && (!word_start || *in->cursor != '~'))
                in->cursor++;
            we = we_literal(st, s, in->cursor - s);
            break;
        }
        }

        word_start = false;
        if (we) {
            we_append(tail, we);
            tail = &we->next;
        }
    }

    return word;
}
