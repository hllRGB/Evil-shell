/* vim: set ft=c ts=8 sw=8 sts=8 et: */
#include "parse_main.h"
#include "include/evilgeneral.h"
#include "parse_word.h"
#include <string.h>

/* ── ctx ring ── */

enum { CM = 5 };
#define CC(ctx, h) ((ctx)[(h) % CM])
#define CN(ctx, h) ((ctx)[((h) + 1) % CM])
#define ADV(h) ((h)++)

/* ── fwd decls ── */

static COMMAND_T * nullable parse_inputunit(INPUT * nonnull,
                                            TOKEN_T * nonnull,
                                            size_t * nonnull,
                                            STAREA_T * nonnull);
static COMMAND_T * nullable parse_compound_list(INPUT * nonnull,
                                                TOKEN_T * nonnull,
                                                size_t * nonnull,
                                                STAREA_T * nonnull);
static COMMAND_T * nullable parse_list1(INPUT * nonnull,
                                        TOKEN_T * nonnull,
                                        size_t * nonnull,
                                        STAREA_T * nonnull);
static COMMAND_T * nullable parse_pipeline_command(INPUT * nonnull,
                                                   TOKEN_T * nonnull,
                                                   size_t * nonnull,
                                                   STAREA_T * nonnull);
static COMMAND_T * nullable parse_pipeline(INPUT * nonnull,
                                           TOKEN_T * nonnull,
                                           size_t * nonnull,
                                           STAREA_T * nonnull);
static COMMAND_T * nullable parse_command(INPUT * nonnull,
                                          TOKEN_T * nonnull,
                                          size_t * nonnull,
                                          STAREA_T * nonnull);
static COMMAND_T * nullable parse_simple_command(INPUT * nonnull,
                                                 TOKEN_T * nonnull,
                                                 size_t * nonnull,
                                                 STAREA_T * nonnull);
static COMMAND_T * nullable parse_shell_command(INPUT * nonnull,
                                                TOKEN_T * nonnull,
                                                size_t * nonnull,
                                                STAREA_T * nonnull);
static COMMAND_T * nullable parse_for(INPUT * nonnull,
                                      TOKEN_T * nonnull,
                                      size_t * nonnull,
                                      STAREA_T * nonnull);
static COMMAND_T * nullable parse_while(INPUT * nonnull,
                                        TOKEN_T * nonnull,
                                        size_t * nonnull,
                                        STAREA_T * nonnull);
static COMMAND_T * nullable parse_until(INPUT * nonnull,
                                        TOKEN_T * nonnull,
                                        size_t * nonnull,
                                        STAREA_T * nonnull);
static COMMAND_T * nullable parse_if(INPUT * nonnull,
                                     TOKEN_T * nonnull,
                                     size_t * nonnull,
                                     STAREA_T * nonnull);
static COMMAND_T * nullable parse_case(INPUT * nonnull,
                                       TOKEN_T * nonnull,
                                       size_t * nonnull,
                                       STAREA_T * nonnull);
static COMMAND_T * nullable parse_select(INPUT * nonnull,
                                         TOKEN_T * nonnull,
                                         size_t * nonnull,
                                         STAREA_T * nonnull);
static COMMAND_T * nullable parse_subshell(INPUT * nonnull,
                                           TOKEN_T * nonnull,
                                           size_t * nonnull,
                                           STAREA_T * nonnull);
static COMMAND_T * nullable parse_group(INPUT * nonnull,
                                        TOKEN_T * nonnull,
                                        size_t * nonnull,
                                        STAREA_T * nonnull);
static COMMAND_T * nullable parse_coproc(INPUT * nonnull,
                                         TOKEN_T * nonnull,
                                         size_t * nonnull,
                                         STAREA_T * nonnull);
static COMMAND_T * nullable parse_function_def(INPUT * nonnull,
                                               TOKEN_T * nonnull,
                                               size_t * nonnull,
                                               STAREA_T * nonnull,
                                               bool kw);
static COMMAND_T * nullable parse_cond(INPUT * nonnull,
                                       TOKEN_T * nonnull,
                                       size_t * nonnull,
                                       STAREA_T * nonnull);
static COMMAND_T * nullable parse_arith(INPUT * nonnull,
                                        TOKEN_T * nonnull,
                                        size_t * nonnull,
                                        STAREA_T * nonnull);

/* ── keyword table ── */

typedef struct {
        const char * name;
        TOK_TYPE tok;
} KW;
static const KW kws[] = {
    {"!",        TOK_BANG      },
    {"[[",       TOK_COND_START},
    {"]]",       TOK_COND_END  },
    {"if",       TOK_IF        },
    {"then",     TOK_THEN      },
    {"else",     TOK_ELSE      },
    {"elif",     TOK_ELIF      },
    {"fi",       TOK_FI        },
    {"case",     TOK_CASE      },
    {"esac",     TOK_ESAC      },
    {"for",      TOK_FOR       },
    {"select",   TOK_SELECT    },
    {"while",    TOK_WHILE     },
    {"until",    TOK_UNTIL     },
    {"do",       TOK_DO        },
    {"done",     TOK_DONE      },
    {"function", TOK_FUNCTION  },
    {"coproc",   TOK_COPROC    },
    {"in",       TOK_IN        },
    {"time",     TOK_TIME      },
};

static TOK_TYPE kwd(const char * nonnull s, size_t n) {
        for (size_t i = 0; i < sizeof(kws) / sizeof(kws[0]); i++) {
                size_t ln = strlen(kws[i].name);
                if (ln == n && memcmp(s, kws[i].name, n) == 0)
                        return kws[i].tok;
        }
        return TOK_WORD;
}

/* ── lex ── */

static bool is_ws(char c) {
        return c == ' ' || c == '\t';
}

TOKEN_T * nonnull lex(INPUT * nonnull in,
                      TOKEN_T * nonnull ctx,
                      uint8_t flags,
                      STAREA_T * nonnull st) {
        (void)st;
        TOKEN_T * tok = &ctx[0]; /* always write to slot 0; lex1 rotates */

again:
        while (in->cursor < in->end && is_ws(*in->cursor))
                in->cursor++;
        if (in->cursor < in->end && *in->cursor == '#') {
                while (in->cursor < in->end && *in->cursor != '\n')
                        in->cursor++;
                goto again;
        }
        tok->start = in->cursor;
        tok->end   = NULL;
        if (in->cursor >= in->end) {
                tok->type = TOK_EOF;
                tok->end  = in->cursor;
                return tok;
        }
        char c = *in->cursor;
        if (c == '\n') {
                if (flags & CHKNL) {
                        in->cursor++;
                        goto again;
                }
                in->cursor++;
                tok->type = TOK_NEWLINE;
                tok->end  = in->cursor;
                return tok;
        }
        switch (c) {
        case ';':
                in->cursor++;
                if (in->cursor < in->end && *in->cursor == ';') {
                        in->cursor++;
                        tok->type = (in->cursor < in->end && *in->cursor == '&')
                                        ? (in->cursor++, TOK_DSEMI_AMP)
                                        : TOK_DSEMI;
                } else if (in->cursor < in->end && *in->cursor == '&') {
                        in->cursor++;
                        tok->type = TOK_SEMI_AMP;
                } else {
                        tok->type = TOK_SEMI;
                }
                break;
        case '&':
                in->cursor++;
                tok->type = (in->cursor < in->end && *in->cursor == '&')
                                ? (in->cursor++, TOK_AND_AND)
                                : TOK_AMP;
                break;
        case '|':
                in->cursor++;
                if (in->cursor < in->end && *in->cursor == '|') {
                        in->cursor++;
                        tok->type = TOK_OR_OR;
                } else if (in->cursor < in->end && *in->cursor == '&') {
                        in->cursor++;
                        tok->type = TOK_BAR_AND;
                } else
                        tok->type = TOK_PIPE;
                break;
        case '(':
                in->cursor++;
                tok->type = (in->cursor < in->end && *in->cursor == '(')
                                ? (in->cursor++, TOK_ARITH_CMD)
                                : TOK_LPAREN;
                break;
        case ')':
                in->cursor++;
                tok->type = TOK_RPAREN;
                break;
        case '<':
        case '>':
                goto redir;
        default:
                goto word;
        }
        tok->end = in->cursor;
        return tok;

redir: {
        int inst = REDIR_OUT;
        if (*in->cursor == '>') {
                in->cursor++;
                if (in->cursor < in->end && *in->cursor == '>') {
                        in->cursor++;
                        inst = REDIR_APPEND;
                } else if (in->cursor < in->end && *in->cursor == '&') {
                        in->cursor++;
                        inst = REDIR_ERR_OUT;
                } else if (in->cursor < in->end && *in->cursor == '|') {
                        in->cursor++;
                        inst = REDIR_OUT_FORCE;
                }
        } else {
                in->cursor++;
                if (in->cursor < in->end && *in->cursor == '<') {
                        in->cursor++;
                        if (in->cursor < in->end && *in->cursor == '-') {
                                in->cursor++;
                                inst = REDIR_DEBLANK_READ;
                        } else if (in->cursor < in->end && *in->cursor == '<') {
                                in->cursor++;
                                inst = REDIR_READ_STRING;
                        } else
                                inst = REDIR_READ_UNTIL;
                } else if (in->cursor < in->end && *in->cursor == '>') {
                        in->cursor++;
                        inst = REDIR_IN_OUT;
                } else if (in->cursor < in->end && *in->cursor == '&') {
                        in->cursor++;
                        inst = REDIR_DUP_IN;
                } else
                        inst = REDIR_IN;
        }
        tok->type                   = TOK_REDIR;
        tok->data.redir.fd          = -1;
        tok->data.redir.instruction = inst;
        tok->end                    = in->cursor;
        return tok;
}

word:;
        if (c >= '0' && c <= '9') {
                const char * np = in->cursor;
                while (np < in->end && *np >= '0' && *np <= '9')
                        np++;
                if (np < in->end && (*np == '>' || *np == '<')) {
                        int fd = 0;
                        for (const char * p = in->cursor; p < np; p++)
                                fd = fd * 10 + (*p - '0');
                        in->cursor = np;
                        int inst   = REDIR_OUT;
                        if (*in->cursor == '>') {
                                in->cursor++;
                                if (in->cursor < in->end
                                    && *in->cursor == '>') {
                                        in->cursor++;
                                        inst = REDIR_APPEND;
                                } else if (in->cursor < in->end
                                           && *in->cursor == '&') {
                                        in->cursor++;
                                        inst = REDIR_ERR_OUT;
                                } else if (in->cursor < in->end
                                           && *in->cursor == '|') {
                                        in->cursor++;
                                        inst = REDIR_OUT_FORCE;
                                }
                        } else {
                                in->cursor++;
                                if (in->cursor < in->end
                                    && *in->cursor == '<') {
                                        in->cursor++;
                                        inst = REDIR_READ_UNTIL;
                                } else if (in->cursor < in->end
                                           && *in->cursor == '>') {
                                        in->cursor++;
                                        inst = REDIR_IN_OUT;
                                } else if (in->cursor < in->end
                                           && *in->cursor == '&') {
                                        in->cursor++;
                                        inst = REDIR_DUP_IN;
                                }
                        }
                        tok->type                   = TOK_REDIR;
                        tok->data.redir.fd          = fd;
                        tok->data.redir.instruction = inst;
                        tok->end                    = in->cursor;
                        return tok;
                }
        }

        /* read word chars */
        const char * ws = in->cursor;
        while (in->cursor < in->end) {
                char wc = *in->cursor;
                if (is_ws(wc) || wc == '\n' || wc == '|' || wc == '&'
                    || wc == ';' || wc == '(' || wc == ')' || wc == '<'
                    || wc == '>')
                        break;
                in->cursor++;
        }
        size_t wn = (size_t)(in->cursor - ws);
        if (flags & CHKKWD) {
                TOK_TYPE kt = kwd(ws, wn);
                if (kt != TOK_WORD) {
                        tok->type  = kt;
                        tok->start = ws;
                        tok->end   = in->cursor;
                        return tok;
                }
        }
        /* parse word via parse_word */
        INPUT si       = *in;
        si.cursor      = ws;
        si.end         = in->cursor;
        WORD_T * w     = parse_word(&si, ctx, st);
        tok->type      = TOK_WORD;
        tok->start     = ws;
        tok->end       = in->cursor;
        tok->data.word = w ? *w : (WORD_T){0};
        return tok;
}

/* ── helpers ── */

static COMMAND_T * nonnull cmd_new(CMD_TYPE t, STAREA_T * nonnull s) {
        COMMAND_T * c = stalloc(s, sizeof(COMMAND_T));
        memset(c, 0, sizeof(COMMAND_T));
        c->type = t;
        return c;
}
static void redir_add(COMMAND_T * nonnull c, REDIR_T * nonnull r) {
        REDIR_T ** p = &c->redirects;
        while (*p)
                p = &(*p)->next;
        *p = r;
}
static void nl(INPUT * nonnull in, TOKEN_T * nonnull ctx, size_t * nonnull h) {
        (void)ctx;
        (void)h;
        /* skip whitespace + newlines */
        while (in->cursor < in->end
               && (*in->cursor == ' ' || *in->cursor == '\t'
                   || *in->cursor == '\n'))
                in->cursor++;
}
static TOKEN_T * nonnull lex1(INPUT * nonnull in,
                              TOKEN_T * nonnull ctx,
                              size_t * nonnull h,
                              uint8_t f,
                              STAREA_T * nonnull s) {
        TOKEN_T * t    = lex(in, ctx, f, s);
        ctx[(*h) & CM] = *t;
        ADV(*h);
        return &CC(ctx, *h);
}
static TOKEN_T * nonnull cur_tok(TOKEN_T * nonnull ctx, size_t h) {
        (void)ctx;
        return &CC(ctx, h);
}
/* unlex: undo the last lex; rs = saved start pos */
static void unlex(INPUT * nonnull in,
                  TOKEN_T * nonnull ctx,
                  size_t * nonnull h,
                  const char * nonnull rs) {
        in->cursor = rs;
        if (*h > 0)
                (*h)--;
}

/* ── parse_inputunit ── */

static COMMAND_T * nullable parse_inputunit(INPUT * nonnull in,
                                            TOKEN_T * nonnull ctx,
                                            size_t * nonnull h,
                                            STAREA_T * nonnull s) {
        nl(in, ctx, h);
        TOKEN_T * t = lex1(in, ctx, h, CHKNL, s);
        if (t->type == TOK_EOF || t->type == TOK_NEWLINE)
                return NULL;
        unlex(in, ctx, h, t->start);
        return parse_compound_list(in, ctx, h, s);
}

/* ── compound_list / list1 ── */

static COMMAND_T * nullable parse_compound_list(INPUT * nonnull in,
                                                TOKEN_T * nonnull ctx,
                                                size_t * nonnull h,
                                                STAREA_T * nonnull s) {
        nl(in, ctx, h);
        return parse_list1(in, ctx, h, s);
}

static COMMAND_T * nullable parse_list1(INPUT * nonnull in,
                                        TOKEN_T * nonnull ctx,
                                        size_t * nonnull h,
                                        STAREA_T * nonnull s) {
        COMMAND_T * l = parse_pipeline_command(in, ctx, h, s);
        if (!l)
                return NULL;
        for (;;) {
                TOKEN_T * t = lex1(in, ctx, h, CHKNL, s);
                int co      = 0;
                switch (t->type) {
                case TOK_AND_AND:
                        co = CONN_AND_AND;
                        break;
                case TOK_OR_OR:
                        co = CONN_OR_OR;
                        break;
                case TOK_SEMI:
                        co = CONN_SEMI;
                        break;
                case TOK_AMP:
                        co = CONN_AMP;
                        break;
                case TOK_NEWLINE:
                        co = CONN_SEMI;
                        break;
                default:
                        unlex(in, ctx, h, t->start);
                        return l;
                }
                nl(in, ctx, h);
                COMMAND_T * r = parse_pipeline_command(in, ctx, h, s);
                if (!r)
                        return l;
                COMMAND_T * n = cmd_new(CMD_CONNECTION, s);
                struct connection_cmd * cc
                    = stalloc(s, sizeof(struct connection_cmd));
                cc->left           = l;
                cc->right          = r;
                cc->connector      = co;
                n->body.connection = cc;
                l                  = n;
        }
}

/* ── pipeline_command / pipeline ── */

static COMMAND_T * nullable parse_pipeline_command(INPUT * nonnull in,
                                                   TOKEN_T * nonnull ctx,
                                                   size_t * nonnull h,
                                                   STAREA_T * nonnull s) {
        uint32_t pf = 0;
        TOKEN_T * t = lex1(in, ctx, h, CHKNL, s);
        if (t->type == TOK_BANG) {
                pf |= CMD_INVERT_RETURN;
                t = lex1(in, ctx, h, CHKNL, s);
        }
        if (t->type == TOK_TIME) {
                pf |= CMD_TIME_PIPELINE;
                t = lex1(in, ctx, h, CHKNL, s);
        }
        unlex(in, ctx, h, t->start);
        COMMAND_T * c = parse_pipeline(in, ctx, h, s);
        if (c)
                c->flags |= pf;
        return c;
}

static COMMAND_T * nullable parse_pipeline(INPUT * nonnull in,
                                           TOKEN_T * nonnull ctx,
                                           size_t * nonnull h,
                                           STAREA_T * nonnull s) {
        COMMAND_T * l = parse_command(in, ctx, h, s);
        if (!l)
                return NULL;
        for (;;) {
                TOKEN_T * t = lex1(in, ctx, h, CHKNL, s);
                if (t->type != TOK_PIPE && t->type != TOK_BAR_AND) {
                        unlex(in, ctx, h, t->start);
                        return l;
                }
                int co = (t->type == TOK_PIPE) ? CONN_PIPE : CONN_BAR_AND;
                nl(in, ctx, h);
                COMMAND_T * r = parse_command(in, ctx, h, s);
                if (!r)
                        return l;
                COMMAND_T * n = cmd_new(CMD_CONNECTION, s);
                struct connection_cmd * cc
                    = stalloc(s, sizeof(struct connection_cmd));
                cc->left           = l;
                cc->right          = r;
                cc->connector      = co;
                n->body.connection = cc;
                l                  = n;
        }
}

/* ── command ── */

static COMMAND_T * nullable parse_command(INPUT * nonnull in,
                                          TOKEN_T * nonnull ctx,
                                          size_t * nonnull h,
                                          STAREA_T * nonnull s) {
        TOKEN_T * t = lex1(in, ctx, h, CHKKWD, s);
        switch (t->type) {
        case TOK_FUNCTION:
                return parse_function_def(in, ctx, h, s, true);
        case TOK_WORD: {
                const char * rs = t->start;
                TOKEN_T * la    = lex1(in, ctx, h, 0, s);
                if (la->type == TOK_LPAREN) {
                        TOKEN_T * la2 = lex1(in, ctx, h, 0, s);
                        if (la2->type == TOK_RPAREN) {
                                unlex(in, ctx, h, rs);
                                lex1(in,
                                     ctx,
                                     h,
                                     CHKKWD,
                                     s); /* re-lex the WORD */
                                return parse_function_def(in, ctx, h, s, false);
                        }
                        unlex(in, ctx, h, rs);
                } else {
                        unlex(in, ctx, h, rs);
                }
                return parse_simple_command(in, ctx, h, s);
        }
        case TOK_ASSIGNMENT_WORD:
        case TOK_REDIR:
                return parse_simple_command(in, ctx, h, s);
        case TOK_IF:
                return parse_if(in, ctx, h, s);
        case TOK_WHILE:
                return parse_while(in, ctx, h, s);
        case TOK_UNTIL:
                return parse_until(in, ctx, h, s);
        case TOK_FOR:
                return parse_for(in, ctx, h, s);
        case TOK_SELECT:
                return parse_select(in, ctx, h, s);
        case TOK_CASE:
                return parse_case(in, ctx, h, s);
        case TOK_LPAREN:
                return parse_subshell(in, ctx, h, s);
        case TOK_COND_START:
                return parse_cond(in, ctx, h, s);
        case TOK_ARITH_CMD:
                return parse_arith(in, ctx, h, s);
        case TOK_COPROC:
                return parse_coproc(in, ctx, h, s);
        default:
                unlex(in, ctx, h, t->start);
                return NULL;
        }
}

/* ── simple_command ── */

static COMMAND_T * nullable parse_simple_command(INPUT * nonnull in,
                                                 TOKEN_T * nonnull ctx,
                                                 size_t * nonnull h,
                                                 STAREA_T * nonnull s) {
        COMMAND_T * cmd   = cmd_new(CMD_SIMPLE, s);
        SIMPLE_CMD_T * sc = stalloc(s, sizeof(SIMPLE_CMD_T));
        memset(sc, 0, sizeof(SIMPLE_CMD_T));
        WORD_LIST_T ** wt = &sc->words;
        for (;;) {
                TOKEN_T * t = lex1(in, ctx, h, 0, s);
                switch (t->type) {
                case TOK_WORD:
                case TOK_ASSIGNMENT_WORD: {
                        WORD_LIST_T * wl = stalloc(s, sizeof(WORD_LIST_T));
                        wl->word         = t->data.word;
                        wl->next         = NULL;
                        *wt              = wl;
                        wt               = &wl->next;
                        continue;
                }
                case TOK_REDIR: {
                        REDIR_T * r = stalloc(s, sizeof(REDIR_T));
                        memset(r, 0, sizeof(REDIR_T));
                        r->instruction
                            = (enum redir_instruction)t->data.redir.instruction;
                        r->redirector.dest = t->data.redir.fd;
                        TOKEN_T * rt       = lex1(in, ctx, h, 0, s);
                        if (rt->type == TOK_WORD) {
                                WORD_T * wn        = stalloc(s, sizeof(WORD_T));
                                *wn                = rt->data.word;
                                r->redirectee.path = wn;
                        }
                        redir_add(cmd, r);
                        continue;
                }
                default:
                        unlex(in, ctx, h, t->start);
                        goto done;
                }
        }
done:
        cmd->body.simple = sc;
        return cmd;
}

/* ── function_def ── */

static COMMAND_T * nullable parse_function_def(INPUT * nonnull in,
                                               TOKEN_T * nonnull ctx,
                                               size_t * nonnull h,
                                               STAREA_T * nonnull s,
                                               bool kw) {
        if (kw)
                lex1(in, ctx, h, CHKKWD, s); /* FUNCTION */
        TOKEN_T * t              = lex1(in, ctx, h, 0, s);
        struct function_def * fd = stalloc(s, sizeof(struct function_def));
        memset(fd, 0, sizeof(struct function_def));
        fd->name = strndup(t->start, (size_t)(t->end - t->start));
        lex1(in, ctx, h, 0, s); /* ( */
        lex1(in, ctx, h, 0, s); /* ) */
        nl(in, ctx, h);
        fd->body = parse_shell_command(in, ctx, h, s);
        if (!fd->body)
                return NULL;
        for (;;) {
                TOKEN_T * tr = lex1(in, ctx, h, 0, s);
                if (tr->type == TOK_REDIR) {
                        REDIR_T * r = stalloc(s, sizeof(REDIR_T));
                        memset(r, 0, sizeof(REDIR_T));
                        r->instruction = (enum redir_instruction)
                                             tr->data.redir.instruction;
                        TOKEN_T * rt = lex1(in, ctx, h, 0, s);
                        if (rt->type == TOK_WORD) {
                                WORD_T * wn        = stalloc(s, sizeof(WORD_T));
                                *wn                = rt->data.word;
                                r->redirectee.path = wn;
                        }
                        redir_add(fd->body, r);
                } else {
                        unlex(in, ctx, h, tr->start);
                        break;
                }
        }
        COMMAND_T * cmd    = cmd_new(CMD_FUNCTION_DEF, s);
        cmd->body.function = fd;
        return cmd;
}

/* ── shell_command ── */

static COMMAND_T * nullable parse_shell_command(INPUT * nonnull in,
                                                TOKEN_T * nonnull ctx,
                                                size_t * nonnull h,
                                                STAREA_T * nonnull s) {
        TOKEN_T * t = lex1(in, ctx, h, CHKKWD, s);
        switch (t->type) {
        case TOK_FOR:
                return parse_for(in, ctx, h, s);
        case TOK_WHILE:
                return parse_while(in, ctx, h, s);
        case TOK_UNTIL:
                return parse_until(in, ctx, h, s);
        case TOK_IF:
                return parse_if(in, ctx, h, s);
        case TOK_CASE:
                return parse_case(in, ctx, h, s);
        case TOK_SELECT:
                return parse_select(in, ctx, h, s);
        case TOK_LPAREN:
                return parse_subshell(in, ctx, h, s);
        case TOK_COND_START:
                return parse_cond(in, ctx, h, s);
        case TOK_ARITH_CMD:
                return parse_arith(in, ctx, h, s);
        default:
                unlex(in, ctx, h, t->start);
                return NULL;
        }
}

/* ── for ── */

static COMMAND_T * nullable parse_for(INPUT * nonnull in,
                                      TOKEN_T * nonnull ctx,
                                      size_t * nonnull h,
                                      STAREA_T * nonnull s) {
        lex1(in, ctx, h, CHKKWD, s); /* FOR — consumed */
        (void)cur_tok;
        TOKEN_T * t         = lex1(in, ctx, h, 0, s);
        struct for_cmd * fc = stalloc(s, sizeof(struct for_cmd));
        memset(fc, 0, sizeof(struct for_cmd));
        fc->var_name = strndup(t->start, (size_t)(t->end - t->start));
        nl(in, ctx, h);
        t = lex1(in, ctx, h, 0, s);
        if (t->type == TOK_IN) {
                WORD_LIST_T ** wt = &fc->map_list;
                for (;;) {
                        t = lex1(in, ctx, h, 0, s);
                        if (t->type == TOK_SEMI || t->type == TOK_NEWLINE
                            || t->type == TOK_EOF)
                                break;
                        WORD_LIST_T * wl = stalloc(s, sizeof(WORD_LIST_T));
                        wl->word         = t->data.word;
                        wl->next         = NULL;
                        *wt              = wl;
                        wt               = &wl->next;
                }
        } else {
                unlex(in, ctx, h, t->start);
        }
        nl(in, ctx, h);
        t = lex1(in, ctx, h, CHKKWD, s);
        if (t->type == TOK_DO) {
                fc->body = parse_compound_list(in, ctx, h, s);
                lex1(in, ctx, h, CHKKWD, s);
        } else if (t->type == '{') {
                fc->body = parse_compound_list(in, ctx, h, s);
                lex1(in, ctx, h, 0, s);
        }
        COMMAND_T * cmd   = cmd_new(CMD_FOR, s);
        cmd->body.for_cmd = fc;
        return cmd;
}

/* ── while / until ── */

static COMMAND_T * nullable parse_while(INPUT * nonnull in,
                                        TOKEN_T * nonnull ctx,
                                        size_t * nonnull h,
                                        STAREA_T * nonnull s) {
        lex1(in, ctx, h, CHKKWD, s);
        COMMAND_T * cd = parse_compound_list(in, ctx, h, s);
        lex1(in, ctx, h, CHKKWD, s);
        COMMAND_T * bd = parse_compound_list(in, ctx, h, s);
        lex1(in, ctx, h, CHKKWD, s);
        COMMAND_T * c         = cmd_new(CMD_WHILE, s);
        struct while_cmd * wc = stalloc(s, sizeof(struct while_cmd));
        wc->condition         = cd;
        wc->body              = bd;
        c->body.while_cmd     = wc;
        return c;
}
static COMMAND_T * nullable parse_until(INPUT * nonnull in,
                                        TOKEN_T * nonnull ctx,
                                        size_t * nonnull h,
                                        STAREA_T * nonnull s) {
        COMMAND_T * c = parse_while(in, ctx, h, s);
        if (c)
                c->type = CMD_UNTIL;
        return c;
}

/* ── if ── */

static COMMAND_T * nullable parse_if(INPUT * nonnull in,
                                     TOKEN_T * nonnull ctx,
                                     size_t * nonnull h,
                                     STAREA_T * nonnull s) {
        lex1(in, ctx, h, CHKKWD, s);
        COMMAND_T * cd = parse_compound_list(in, ctx, h, s);
        lex1(in, ctx, h, CHKKWD, s);
        COMMAND_T * tb = parse_compound_list(in, ctx, h, s);
        COMMAND_T * eb = NULL;
        TOKEN_T * t    = lex1(in, ctx, h, CHKKWD, s);
        if (t->type == TOK_ELSE) {
                eb = parse_compound_list(in, ctx, h, s);
                lex1(in, ctx, h, CHKKWD, s);
        } else if (t->type == TOK_ELIF) {
                eb = parse_if(in, ctx, h, s);
        }
        /* else: t is FI already */
        COMMAND_T * c      = cmd_new(CMD_IF, s);
        struct if_cmd * ic = stalloc(s, sizeof(struct if_cmd));
        ic->condition      = cd;
        ic->then_branch    = tb;
        ic->else_branch    = eb;
        c->body.if_cmd     = ic;
        return c;
}

/* ── select ── */

static COMMAND_T * nullable parse_select(INPUT * nonnull in,
                                         TOKEN_T * nonnull ctx,
                                         size_t * nonnull h,
                                         STAREA_T * nonnull s) {
        lex1(in, ctx, h, CHKKWD, s);
        TOKEN_T * t            = lex1(in, ctx, h, 0, s);
        struct select_cmd * sc = stalloc(s, sizeof(struct select_cmd));
        memset(sc, 0, sizeof(struct select_cmd));
        sc->var_name = strndup(t->start, (size_t)(t->end - t->start));
        nl(in, ctx, h);
        t = lex1(in, ctx, h, 0, s);
        if (t->type == TOK_IN) {
                WORD_LIST_T ** wt = &sc->map_list;
                for (;;) {
                        t = lex1(in, ctx, h, 0, s);
                        if (t->type == TOK_SEMI || t->type == TOK_NEWLINE
                            || t->type == TOK_EOF)
                                break;
                        WORD_LIST_T * wl = stalloc(s, sizeof(WORD_LIST_T));
                        wl->word         = t->data.word;
                        wl->next         = NULL;
                        *wt              = wl;
                        wt               = &wl->next;
                }
        } else {
                unlex(in, ctx, h, t->start);
        }
        nl(in, ctx, h);
        lex1(in, ctx, h, CHKKWD, s);
        sc->body = parse_compound_list(in, ctx, h, s);
        lex1(in, ctx, h, CHKKWD, s);
        COMMAND_T * c  = cmd_new(CMD_SELECT, s);
        c->body.select = sc;
        return c;
}

/* ── case ── */

static COMMAND_T * nullable parse_case(INPUT * nonnull in,
                                       TOKEN_T * nonnull ctx,
                                       size_t * nonnull h,
                                       STAREA_T * nonnull s) {
        lex1(in, ctx, h, CHKKWD, s);
        TOKEN_T * t          = lex1(in, ctx, h, 0, s);
        struct case_cmd * cc = stalloc(s, sizeof(struct case_cmd));
        memset(cc, 0, sizeof(struct case_cmd));
        WORD_T * ww = stalloc(s, sizeof(WORD_T));
        *ww         = t->data.word;
        cc->word    = ww;
        nl(in, ctx, h);
        lex1(in, ctx, h, 0, s);
        nl(in, ctx, h);
        PATTERN_LIST_T ** pt = &cc->clauses;
        for (;;) {
                t = lex1(in, ctx, h, 0, s);
                if (t->type == TOK_ESAC)
                        break;
                unlex(in, ctx, h, t->start);
                PATTERN_LIST_T * pl = stalloc(s, sizeof(PATTERN_LIST_T));
                memset(pl, 0, sizeof(PATTERN_LIST_T));
                WORD_LIST_T ** wt = &pl->patterns;
                for (;;) {
                        t = lex1(in, ctx, h, 0, s);
                        if (t->type == TOK_RPAREN)
                                break;
                        WORD_LIST_T * wl = stalloc(s, sizeof(WORD_LIST_T));
                        wl->word         = t->data.word;
                        wl->next         = NULL;
                        *wt              = wl;
                        wt               = &wl->next;
                }
                pl->action = parse_compound_list(in, ctx, h, s);
                t          = lex1(in, ctx, h, 0, s);
                if (t->type == TOK_DSEMI)
                        pl->flags = CASEPAT_FALLTHROUGH;
                else if (t->type == TOK_SEMI_AMP)
                        pl->flags = CASEPAT_FALLTHROUGH;
                else if (t->type == TOK_DSEMI_AMP)
                        pl->flags = (CASEPAT_FLAGS)(CASEPAT_FALLTHROUGH
                                                    | CASEPAT_TESTNEXT);
                else
                        unlex(in, ctx, h, t->start);
                nl(in, ctx, h);
                *pt = pl;
                pt  = &pl->next;
        }
        COMMAND_T * c    = cmd_new(CMD_CASE, s);
        c->body.case_cmd = cc;
        return c;
}

/* ── subshell / group / coproc ── */

static COMMAND_T * nullable parse_subshell(INPUT * nonnull in,
                                           TOKEN_T * nonnull ctx,
                                           size_t * nonnull h,
                                           STAREA_T * nonnull s) {
        lex1(in, ctx, h, CHKNL, s);
        COMMAND_T * bd = parse_compound_list(in, ctx, h, s);
        lex1(in, ctx, h, 0, s);
        COMMAND_T * c            = cmd_new(CMD_SUBSHELL, s);
        struct subshell_cmd * sc = stalloc(s, sizeof(struct subshell_cmd));
        sc->body                 = bd;
        c->body.subshell         = sc;
        return c;
}
static COMMAND_T * nullable parse_group(INPUT * nonnull in,
                                        TOKEN_T * nonnull ctx,
                                        size_t * nonnull h,
                                        STAREA_T * nonnull s) {
        lex1(in, ctx, h, 0, s);
        const char * rs = cur_tok(ctx, *h)->start;
        unlex(in, ctx, h, rs);
        COMMAND_T * bd = parse_compound_list(in, ctx, h, s);
        lex1(in, ctx, h, 0, s);
        unlex(in, ctx, h, cur_tok(ctx, *h)->start);
        COMMAND_T * c         = cmd_new(CMD_GROUP, s);
        struct group_cmd * gc = stalloc(s, sizeof(struct group_cmd));
        gc->body              = bd;
        c->body.group         = gc;
        return c;
}
static COMMAND_T * nullable parse_coproc(INPUT * nonnull in,
                                         TOKEN_T * nonnull ctx,
                                         size_t * nonnull h,
                                         STAREA_T * nonnull s) {
        lex1(in, ctx, h, CHKKWD, s);
        struct coproc_cmd * cp = stalloc(s, sizeof(struct coproc_cmd));
        memset(cp, 0, sizeof(struct coproc_cmd));
        TOKEN_T * t = lex1(in, ctx, h, CHKKWD, s);
        if (t->type == TOK_WORD) {
                cp->name = strndup(t->start, (size_t)(t->end - t->start));
        } else
                unlex(in, ctx, h, t->start);
        cp->body       = parse_shell_command(in, ctx, h, s);
        COMMAND_T * c  = cmd_new(CMD_COPROC, s);
        c->body.coproc = cp;
        return c;
}

/* ── cond / arith (stubs) ── */

static COMMAND_T * nullable parse_cond(INPUT * nonnull in,
                                       TOKEN_T * nonnull ctx,
                                       size_t * nonnull h,
                                       STAREA_T * nonnull s) {
        (void)in;
        (void)ctx;
        (void)h;
        (void)s;
        return cmd_new(CMD_COND, s);
}
static COMMAND_T * nullable parse_arith(INPUT * nonnull in,
                                        TOKEN_T * nonnull ctx,
                                        size_t * nonnull h,
                                        STAREA_T * nonnull s) {
        (void)in;
        (void)ctx;
        (void)h;
        (void)s;
        return cmd_new(CMD_ARITH, s);
}

/* ── parse_main ── */

COMMAND_T * nullable parse_main(INPUT * nonnull in,
                                TOKEN_T * nonnull ctx,
                                STAREA_T * nonnull st) {
        size_t h = 0;
        memset(ctx, 0, sizeof(TOKEN_T) * 5);
        COMMAND_T * r = parse_inputunit(in, ctx, &h, st);
        return r;
}
