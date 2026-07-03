/* vim: set ts=8 sw=8 nu rnu sts=8 et: */
#ifndef SH_SIGNALS_H
#define SH_SIGNALS_H

#include "include/evilgeneral.h"

nodiscard SUCCESS_T sh_signal_init(void);
void sh_signal_shutdown(void);

nodiscard int sh_signal_self_pipe_fd(void);
nodiscard SUCCESS_T sh_signal_drain(void);

nodiscard bool sh_signal_is_pending(int signo);
nodiscard bool sh_signal_take_pending(int signo);
void sh_signal_clear_pending(int signo);
void sh_signal_clear_all_pending(void);

nodiscard SUCCESS_T sh_signal_block(int signo, sigset_t * nullable oldmask);
nodiscard SUCCESS_T sh_signal_block_handled(sigset_t * nullable oldmask);
nodiscard SUCCESS_T sh_signal_block_child(sigset_t * nullable oldmask);
nodiscard SUCCESS_T sh_signal_restore_mask(const sigset_t * nonnull oldmask);

nodiscard SUCCESS_T sh_signal_reset_for_child(void);

#endif /* SH_SIGNALS_H */
