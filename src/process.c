/* vim: set ts=8 sw=8 nu rnu sts=8 et: */
#include "include/evilgeneral.h"

#include "libs/memm.h"
#include "process.h"
#include "signals.h"

#ifndef WCOREDUMP
#define WCOREDUMP(status) ((status) & 0x80)
#endif

// ——— internal helpers ———

static PROC_T * nonnull s_proc_alloc(pid_t pid, const char * nonnull name) {
        PROC_T * proc = xmalloc(sizeof(*proc));

        proc->dead_next   = NULL;
        proc->pid         = pid;
        proc->state       = PROC_STATE_RUNNING;
        proc->wait_status = 0;
        proc->exit_status = 0;
        proc->dumped_core = false;

        size_t nlen = strlen(name);
        if (nlen >= sizeof(proc->name))
                nlen = sizeof(proc->name) - 1;
        memcpy(proc->name, name, nlen);
        proc->name[nlen] = '\0';

        return proc;
}

static void s_dead_push(PROC_TABLE_T * nonnull table, PROC_T * nonnull proc) {
        proc->dead_next = NULL;
        if (table->dead.tail != NULL)
                table->dead.tail->dead_next = proc;
        else
                table->dead.head = proc;
        table->dead.tail = proc;
        table->dead.count++;
}

// ——— public API ———

void proc_table_init(PROC_TABLE_T * nonnull table) {
        table->live.tab   = hash_create(64);
        table->live.count = 0;
        table->dead.head  = NULL;
        table->dead.tail  = NULL;
        table->dead.count = 0;
}

void proc_table_destroy(PROC_TABLE_T * nonnull table) {
        HASH_ENTRY_T * entry = table->live.tab->head;

        while (entry != NULL) {
                if (entry->value != NULL)
                        xfree(entry->value);
                entry = entry->iter_next;
        }
        hash_destroy(table->live.tab, NULL);
        table->live.tab   = NULL;
        table->live.count = 0;

        PROC_T * proc = table->dead.head;

        while (proc != NULL) {
                PROC_T * next = proc->dead_next;

                xfree(proc);
                proc = next;
        }
        table->dead.head  = NULL;
        table->dead.tail  = NULL;
        table->dead.count = 0;
}

PROC_CREATE_RESULT_T proc_create(PROC_TABLE_T * nonnull table,
                                 const PROC_CREATE_ATTR_T * nonnull attr) {
        sigset_t oldmask;

        if (sh_signal_block_child(&oldmask) != SUCCESS)
                return PROC_CREATE_FAIL;

        pid_t pid = fork();

        if (pid == -1) {
                (void)sh_signal_restore_mask(&oldmask);
                return PROC_CREATE_FAIL;
        }

        if (pid == 0) {
                (void)sh_signal_reset_for_child();

                for (size_t i = 0; i < attr->extra_close_count; i++)
                        close(attr->extra_close[i]);

                int r = attr->child_setup(attr->child_setup_arg);

                _exit(r == SUCCESS ? 0 : 127);
        }

        *attr->out_pid = pid;

        PROC_T * proc = s_proc_alloc(pid, attr->debug_name);
        uint64_t hash = (uint64_t)pid;

        hash_set_with_hash(table->live.tab, &pid, sizeof(pid), hash, proc);
        table->live.count++;

        (void)sh_signal_restore_mask(&oldmask);
        return PROC_CREATE_OK;
}

bool proc_reap_pending(PROC_TABLE_T * nonnull table) {
        if (!sh_signal_is_pending(SIGCHLD))
                return false;

        (void)sh_signal_take_pending(SIGCHLD);

        bool reaped = false;
        int status;
        pid_t pid;

        while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
                uint64_t h    = (uint64_t)pid;
                PROC_T * proc = hash_get_with_hash(table->live.tab, &pid, sizeof(pid), h);

                if (proc == NULL)
                        continue;

                if (WIFEXITED(status)) {
                        proc->state       = PROC_STATE_EXITED;
                        proc->wait_status = status;
                        proc->exit_status = WEXITSTATUS(status);
                        proc->dumped_core = false;

                        hash_delete_with_hash(table->live.tab, &pid, sizeof(pid), h, NULL);
                        table->live.count--;
                        s_dead_push(table, proc);
                        reaped = true;
                } else if (WIFSIGNALED(status)) {
                        proc->state       = PROC_STATE_SIGNALED;
                        proc->wait_status = status;
                        proc->exit_status = WTERMSIG(status);
                        proc->dumped_core = WCOREDUMP(status);

                        hash_delete_with_hash(table->live.tab, &pid, sizeof(pid), h, NULL);
                        table->live.count--;
                        s_dead_push(table, proc);
                        reaped = true;
                } else if (WIFSTOPPED(status)) {
                        proc->state       = PROC_STATE_STOPPED;
                        proc->wait_status = status;
                        reaped            = true;
                } else if (WIFCONTINUED(status)) {
                        proc->state = PROC_STATE_CONTINUED;
                        reaped      = true;
                }
        }

        return reaped;
}

size_t proc_dead_consume(PROC_TABLE_T * nonnull table,
                         PROC_DEAD_CONSUMER_T nonnull consumer,
                         void * nullable arg) {
        PROC_T * proc = table->dead.head;
        size_t count  = 0;

        table->dead.head  = NULL;
        table->dead.tail  = NULL;
        table->dead.count = 0;

        while (proc != NULL) {
                PROC_T * next = proc->dead_next;

                proc->dead_next = NULL;
                consumer(proc, arg);
                xfree(proc);
                proc = next;
                count++;
        }

        return count;
}

PROC_T * nullable proc_find(PROC_TABLE_T * nonnull table, pid_t pid) {
        uint64_t hash = (uint64_t)pid;

        return hash_get_with_hash(table->live.tab, &pid, sizeof(pid), hash);
}

size_t proc_live_count(PROC_TABLE_T * nonnull table) { return table->live.count; }

size_t proc_dead_count(PROC_TABLE_T * nonnull table) { return table->dead.count; }
