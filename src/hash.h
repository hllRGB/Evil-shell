// vim: set sw=8 ts=8 et:
/* hash.h — evilshell hash API */
#ifndef SH_HASH_H
#define SH_HASH_H

#include "include/evilgeneral.h"

typedef struct hash_entry_str {
        struct hash_entry_str * nullable bucket_prev;
        struct hash_entry_str * nullable bucket_next;
        struct hash_entry_str * nullable iter_prev;
        struct hash_entry_str * nullable iter_next;
        uint64_t hash;
        size_t key_len;
        void * nullable value;
        uint8_t key[];
} HASH_ENTRY_T;

typedef struct hashtab_str {
        HASH_ENTRY_T * nullable * nonnull buckets;
        HASH_ENTRY_T * nullable head;
        size_t bucket_count;
        size_t size;
} HASHTAB_T;

nodiscard uint64_t hash_calculate(const void * restrict nonnull buf,
                                  size_t len);

nodiscard HASHTAB_T * nonnull hash_create(size_t bucket_count);

void hash_destroy(HASHTAB_T * nullable table,
                  void (*nullable value_destroy)(void * nullable value));

nodiscard HASH_ENTRY_T * nullable hash_find_entry(HASHTAB_T * nonnull table,
                                                  const void * nonnull key,
                                                  size_t key_len);

nodiscard void * nullable hash_read(HASHTAB_T * nonnull table,
                                    const void * nonnull key,
                                    size_t key_len);

HASH_ENTRY_T * nonnull hash_write(HASHTAB_T * nonnull table,
                                  const void * nonnull key,
                                  size_t key_len,
                                  void * nullable value);

bool hash_rm(HASHTAB_T * nonnull table,
             const void * nonnull key,
             size_t key_len,
             void (*nullable value_destroy)(void * nullable value));

nodiscard HASH_ENTRY_T * nullable hash_next(HASH_ENTRY_T * nonnull entry);

#endif /* SH_HASH_H */
