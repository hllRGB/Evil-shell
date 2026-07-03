// vim: set sw=8 ts=8 et:
/* evil shell 内存管理 */

#include "libs/memm.h"
#include "include/evilgeneral.h"

static void memory_abort(const char * nonnull name) {
        perror(name);
        abort();
}

/* discard ok */ static size_t stfreeblock(STBLOCK_T * nonnull block) {
        auto this         = block;
        auto next         = block->next;
        size_t total_size = 0;
        while (next) {
                total_size += this->capa - this->body;
                xfree(this);
                this = next;
                next = this->next;
        }
        total_size += this->capa - this->body;
        xfree(this);
        return total_size;
}

nodiscard static STBLOCK_T * nonnull stnewblk(size_t size) {
        STBLOCK_T * nonnull block = xmalloc(sizeof(STBLOCK_T) + size);
        block->capa = (block->body = (uint8_t * nonnull)(block + 1)) + size;
        block->next = nullptr;
        return block;
}

/* ----------------------------------------------------------------------------------------------------------------------------------
 */

nodiscard STAREA_T * nonnull stnew(void) {
        auto area       = (STAREA_T * nonnull) xmalloc(sizeof(STAREA_T));
        area->lastblock = area->blocks = stnewblk(STALLOC_BLOCK_SIZE);
        area->flush_size               = STALLOC_BLOCK_SIZE;
        return area;
}

nodiscard void * nonnull stalloc(STAREA_T * nonnull area, uint64_t size) {
        if (area->lastblock->top + size > area->lastblock->capa) {

                area->lastblock = area->lastblock->next = stnewblk(
                    size > area->flush_size ? size : area->flush_size);
        }
        auto ptr = area->lastblock->top;
        area->lastblock->top += size;
        return ptr;
}

/* Note: 1 <= align <= 8 */
nodiscard void * nonnull stalignalloc(STAREA_T * nonnull area,
                                      uint64_t size,
                                      uint8_t align) {

        area->lastblock->top = (uint8_t * nonnull)(
            ((uint64_t)area->lastblock->top + align - 1) & ~(align - 1));

        return stalloc(area, size);
}

void stflush(STAREA_T * nonnull area) {
        area->flush_size = stfreeblock(area->blocks);
        area->lastblock = area->blocks = stnewblk(area->flush_size);
}

void stdestroy(STAREA_T * nonnull area) {
        stfreeblock(area->blocks);
        xfree(area);
}

nodiscard void * nonnull xmalloc(size_t size) {
        void * ptr = malloc(size);
        if (ptr == NULL) {
                memory_abort("xmalloc");
        }
        return ptr;
}

nodiscard void * nonnull xcalloc(size_t count, size_t size) {
        void * ptr = calloc(count, size);
        if (ptr == NULL) {
                memory_abort("xcalloc");
        }
        return ptr;
}

nodiscard void * nonnull xrealloc(void * nonnull ptr, size_t size) {
        void * new_ptr = realloc(ptr, size);
        if (new_ptr == NULL) {
                memory_abort("xrealloc");
        }
        return new_ptr;
}

void xfree(void * nonnull ptr) {
        free(ptr);
}
