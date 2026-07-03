/* vim: set ft=c ts=8 sw=8 sts=8 et: */
#ifndef SH_PARSE_WORD_H
#define SH_PARSE_WORD_H

#include "include/evilgeneral.h"
#include "parse_main.h"

WORD_T * nullable parse_word(INPUT * nonnull in,
                             TOKEN_T * nonnull ctx,
                             STAREA_T * nonnull st);

#endif /* SH_PARSE_WORD_H */
