// bind.h
#ifndef SH_EVILINE_BIND_H
#define SH_EVILINE_BIND_H

#include "include/evilgeneral.h"

#include <stdint.h>

#define FUNCTION 1
#define NODE 2
#define NONE 0

struct bind_node_str {
        uint16_t timeout_us;
        char type; // 暂时那三个define
        void * nullable ptr;
};

typedef struct bind_node_str BIND_NODE_T;

extern BIND_NODE_T * nonnull initialize_keymap(void);

extern int
add_bind(BIND_NODE_T * nonnull root, long timeout, char * nonnull seq, void * nonnull func);

extern int8_t
further_match(BIND_NODE_T * nonnull root, void * nullable * nonnull addr, char byte);

extern int rm_bind(BIND_NODE_T * nonnull root, char * nonnull seq);

#endif /* SH_EVILINE_BIND_H */
