/* vim: set ts=8 sw=8 nu rnu sts=8 et: */
#ifndef SH_PROCESS_H
#define SH_PROCESS_H

#include "include/evilgeneral.h"

#include "hash.h"

typedef enum proc_state {
        PROC_STATE_RUNNING,
        PROC_STATE_STOPPED,
        PROC_STATE_CONTINUED,
        PROC_STATE_EXITED,
        PROC_STATE_SIGNALED,
} PROC_STATE_T;

typedef struct proc_str {
        struct proc_str * nullable dead_next;
        pid_t pid;
        PROC_STATE_T state;
        int wait_status;
        int exit_status;
        bool dumped_core;
        char name[16];
} PROC_T;

typedef struct proc_live_table {
        HASHTAB_T * nonnull tab;
        size_t count;
} PROC_LIVE_TABLE_T;

typedef struct proc_dead_fifo {
        PROC_T * nullable head;
        PROC_T * nullable tail;
        size_t count;
} PROC_DEAD_FIFO_T;

typedef struct proc_table {
        PROC_LIVE_TABLE_T live;
        PROC_DEAD_FIFO_T dead;
} PROC_TABLE_T;

typedef struct proc_create_attr {
        pid_t * nonnull out_pid;
        int (*nonnull child_setup)(void * nullable arg);
        void * nullable child_setup_arg;
        const int * nullable extra_close;
        size_t extra_close_count;
        const char * nonnull debug_name;
} PROC_CREATE_ATTR_T;

typedef enum proc_create_result {
        PROC_CREATE_OK,
        PROC_CREATE_FAIL,
} PROC_CREATE_RESULT_T;

typedef void (*PROC_DEAD_CONSUMER_T)(const PROC_T * nonnull proc, void * nullable arg);

void proc_table_init(PROC_TABLE_T * nonnull table);
void proc_table_destroy(PROC_TABLE_T * nonnull table);

nodiscard PROC_CREATE_RESULT_T
proc_create(PROC_TABLE_T * nonnull table, const PROC_CREATE_ATTR_T * nonnull attr);

nodiscard bool proc_reap_pending(PROC_TABLE_T * nonnull table);

size_t proc_dead_consume(PROC_TABLE_T * nonnull table,
                         PROC_DEAD_CONSUMER_T nonnull consumer,
                         void * nullable arg);

nodiscard PROC_T * nullable proc_find(PROC_TABLE_T * nonnull table, pid_t pid);

nodiscard size_t proc_live_count(PROC_TABLE_T * nonnull table);
nodiscard size_t proc_dead_count(PROC_TABLE_T * nonnull table);

#endif /* SH_PROCESS_H */
