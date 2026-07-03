/* vim: set ts=8 sw=8 nu rnu sts=8 et: */
#include "include/evilgeneral.h"

#include "signals.h"

#ifndef NSIG
#define SH_SIGNAL_COUNT 128
#else
#define SH_SIGNAL_COUNT NSIG
#endif
#define SH_SIGNAL_DRAIN_BUFFER_SIZE ((size_t)128)

enum signal_pipe_index {
        SIGNAL_PIPE_READ,
        SIGNAL_PIPE_WRITE,
        SIGNAL_PIPE_COUNT,
};

static int s_signal_pipe[SIGNAL_PIPE_COUNT]      = {-1, -1};
static volatile sig_atomic_t s_signal_pipe_write = -1;
static volatile sig_atomic_t s_pending[SH_SIGNAL_COUNT];
static struct sigaction s_old_actions[SH_SIGNAL_COUNT];
static bool s_installed[SH_SIGNAL_COUNT];
static sigset_t s_handled_set;
static sigset_t s_child_set;
static sigset_t s_startup_mask;
static bool s_initialized;

static bool s_valid_signo(int signo) { return signo > 0 && signo < SH_SIGNAL_COUNT; }

static void s_close_signal_pipe(void) {
        if (s_signal_pipe[SIGNAL_PIPE_READ] >= 0) {
                close(s_signal_pipe[SIGNAL_PIPE_READ]);
                s_signal_pipe[SIGNAL_PIPE_READ] = -1;
        }

        if (s_signal_pipe[SIGNAL_PIPE_WRITE] >= 0) {
                close(s_signal_pipe[SIGNAL_PIPE_WRITE]);
                s_signal_pipe[SIGNAL_PIPE_WRITE] = -1;
        }

        s_signal_pipe_write = -1;
}

nodiscard static SUCCESS_T s_enable_file_status_flag(int fd, int flag) {
        int flags = fcntl(fd, F_GETFL);

        if (flags < 0)
                return FAIL;
        if (fcntl(fd, F_SETFL, flags | flag) < 0)
                return FAIL;
        return SUCCESS;
}

nodiscard static SUCCESS_T s_enable_file_descriptor_flag(int fd, int flag) {
        int flags = fcntl(fd, F_GETFD);

        if (flags < 0)
                return FAIL;
        if (fcntl(fd, F_SETFD, flags | flag) < 0)
                return FAIL;
        return SUCCESS;
}

nodiscard static SUCCESS_T s_prepare_signal_pipe(void) {
        if (pipe(s_signal_pipe) < 0)
                return FAIL;

        if (s_enable_file_status_flag(s_signal_pipe[SIGNAL_PIPE_READ], O_NONBLOCK) != SUCCESS)
                goto fail;
        if (s_enable_file_status_flag(s_signal_pipe[SIGNAL_PIPE_WRITE], O_NONBLOCK) != SUCCESS)
                goto fail;
        if (s_enable_file_descriptor_flag(s_signal_pipe[SIGNAL_PIPE_READ], FD_CLOEXEC) != SUCCESS)
                goto fail;
        if (s_enable_file_descriptor_flag(s_signal_pipe[SIGNAL_PIPE_WRITE], FD_CLOEXEC) != SUCCESS)
                goto fail;

        s_signal_pipe_write = s_signal_pipe[SIGNAL_PIPE_WRITE];
        return SUCCESS;

fail:
        s_close_signal_pipe();
        return FAIL;
}

static void s_signal_handler(int signo) {
        int old_errno = errno;
        uint8_t byte  = 0;

        if (s_valid_signo(signo))
                s_pending[signo] = 1;

        if (s_signal_pipe_write >= 0) {
                ssize_t ret = write((int)s_signal_pipe_write, &byte, sizeof byte);
                (void)ret;
        }

        errno = old_errno;
}

nodiscard static SUCCESS_T s_install_handler(int signo) {
        struct sigaction action = {0};

        if (!s_valid_signo(signo))
                return FAIL;

        action.sa_handler = s_signal_handler;
        sigemptyset(&action.sa_mask);
        action.sa_flags = 0;

        if (sigaction(signo, &action, &s_old_actions[signo]) < 0)
                return FAIL;

        s_installed[signo] = true;
        sigaddset(&s_handled_set, signo);
        return SUCCESS;
}

static void s_restore_handlers(void) {
        for (int signo = 1; signo < SH_SIGNAL_COUNT; signo++) {
                if (s_installed[signo]) {
                        sigaction(signo, &s_old_actions[signo], NULL);
                        s_installed[signo] = false;
                }
        }
}

nodiscard static SUCCESS_T s_install_common_handlers(void) {
#define INSTALL(signo)                                                                             \
        do {                                                                                       \
                if (s_install_handler(signo) != SUCCESS)                                           \
                        return FAIL;                                                               \
        } while (0)

#ifdef SIGHUP
        INSTALL(SIGHUP);
#endif
#ifdef SIGINT
        INSTALL(SIGINT);
#endif
#ifdef SIGQUIT
        INSTALL(SIGQUIT);
#endif
#ifdef SIGTERM
        INSTALL(SIGTERM);
#endif
#ifdef SIGCHLD
        INSTALL(SIGCHLD);
#endif
#ifdef SIGPIPE
        INSTALL(SIGPIPE);
#endif
#ifdef SIGALRM
        INSTALL(SIGALRM);
#endif
#ifdef SIGUSR1
        INSTALL(SIGUSR1);
#endif
#ifdef SIGUSR2
        INSTALL(SIGUSR2);
#endif
#ifdef SIGTSTP
        INSTALL(SIGTSTP);
#endif
#ifdef SIGTTIN
        INSTALL(SIGTTIN);
#endif
#ifdef SIGTTOU
        INSTALL(SIGTTOU);
#endif
#ifdef SIGCONT
        INSTALL(SIGCONT);
#endif
#ifdef SIGWINCH
        INSTALL(SIGWINCH);
#endif

#undef INSTALL
        return SUCCESS;
}

SUCCESS_T sh_signal_init(void) {
        if (s_initialized)
                return SUCCESS;

        sigemptyset(&s_handled_set);
        sigemptyset(&s_child_set);
        sh_signal_clear_all_pending();

        if (sigprocmask(SIG_SETMASK, NULL, &s_startup_mask) < 0)
                return FAIL;

#ifdef SIGCHLD
        sigaddset(&s_child_set, SIGCHLD);
#endif

        if (s_prepare_signal_pipe() != SUCCESS)
                return FAIL;

        if (s_install_common_handlers() != SUCCESS) {
                s_restore_handlers();
                s_close_signal_pipe();
                sigemptyset(&s_handled_set);
                sigemptyset(&s_child_set);
                return FAIL;
        }

        s_initialized = true;
        return SUCCESS;
}

void sh_signal_shutdown(void) {
        if (!s_initialized && s_signal_pipe[SIGNAL_PIPE_READ] < 0
            && s_signal_pipe[SIGNAL_PIPE_WRITE] < 0)
                return;

        s_restore_handlers();
        s_close_signal_pipe();
        sh_signal_clear_all_pending();
        sigemptyset(&s_handled_set);
        sigemptyset(&s_child_set);
        s_initialized = false;
}

int sh_signal_self_pipe_fd(void) {
        return s_signal_pipe[SIGNAL_PIPE_READ];
}

SUCCESS_T sh_signal_drain(void) {
        uint8_t buffer[SH_SIGNAL_DRAIN_BUFFER_SIZE];

        if (s_signal_pipe[SIGNAL_PIPE_READ] < 0)
                return FAIL;

        for (;;) {
                ssize_t ret = read(s_signal_pipe[SIGNAL_PIPE_READ], buffer, sizeof buffer);

                if (ret > 0)
                        continue;
                if (ret == 0)
                        return SUCCESS;
                if (errno == EINTR)
                        continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                        return SUCCESS;

                return FAIL;
        }
}

bool sh_signal_is_pending(int signo) {
        if (!s_valid_signo(signo))
                return false;

        return s_pending[signo] != 0;
}

bool sh_signal_take_pending(int signo) {
        if (!sh_signal_is_pending(signo))
                return false;

        s_pending[signo] = 0;
        return true;
}

void sh_signal_clear_pending(int signo) {
        if (s_valid_signo(signo))
                s_pending[signo] = 0;
}

void sh_signal_clear_all_pending(void) {
        for (int signo = 0; signo < SH_SIGNAL_COUNT; signo++)
                s_pending[signo] = 0;
}

SUCCESS_T sh_signal_block(int signo, sigset_t * nullable oldmask) {
        sigset_t set;

        if (!s_valid_signo(signo))
                return FAIL;

        sigemptyset(&set);
        sigaddset(&set, signo);

        if (sigprocmask(SIG_BLOCK, &set, oldmask) < 0)
                return FAIL;

        return SUCCESS;
}

SUCCESS_T sh_signal_block_handled(sigset_t * nullable oldmask) {
        if (sigprocmask(SIG_BLOCK, &s_handled_set, oldmask) < 0)
                return FAIL;

        return SUCCESS;
}

SUCCESS_T sh_signal_block_child(sigset_t * nullable oldmask) {
        if (sigprocmask(SIG_BLOCK, &s_child_set, oldmask) < 0)
                return FAIL;

        return SUCCESS;
}

SUCCESS_T sh_signal_restore_mask(const sigset_t * nonnull oldmask) {
        if (sigprocmask(SIG_SETMASK, oldmask, NULL) < 0)
                return FAIL;

        return SUCCESS;
}

SUCCESS_T sh_signal_reset_for_child(void) {
        s_restore_handlers();

        if (sigprocmask(SIG_SETMASK, &s_startup_mask, NULL) < 0)
                return FAIL;

        sh_signal_clear_all_pending();
        return SUCCESS;
}
