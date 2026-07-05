// vim: set sw=8 ts=8 et:
/* evilshell 内存管理器 头文件 */
#ifndef SH_MEMM_H
#define SH_MEMM_H

#include "evilgeneral.h"
#include <stddef.h>

#define STALLOC_BLOCK_SIZE (size_t)4096

typedef struct stblock_str {
        uint8_t * nonnull capa;
        uint8_t * nonnull top;
        uint8_t * nonnull body;
        struct stblock_str * nullable next;
} STBLOCK_T;

typedef struct starea_str {
        STBLOCK_T * nonnull blocks;
        STBLOCK_T * nonnull
            lastblock; // 在stnew()和stflush()时设为blocks的值(第一个块)
        size_t flush_size;
} STAREA_T;

nodiscard STAREA_T * nonnull stnew(void);
void stdestroy(STAREA_T * nonnull area);
nodiscard void * nonnull stalloc(STAREA_T * nonnull area, size_t size);
void stflush(STAREA_T * nonnull area);
nodiscard void * nonnull stalignalloc(STAREA_T * nonnull area,
                                      size_t size,
                                      uint8_t align);

nodiscard void * nonnull xmalloc(size_t size);
nodiscard void * nonnull xcalloc(size_t count, size_t size);
nodiscard void * nonnull xrealloc(void * nonnull ptr, size_t size);
void xfree(void * nonnull ptr);

#endif /* SH_MEMM_H */
