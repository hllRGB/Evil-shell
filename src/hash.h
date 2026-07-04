// vim: set sw=8 ts=8 et:
/* hash.h — evilshell hash API */
#ifndef SH_HASH_H
#define SH_HASH_H

#include "include/evilgeneral.h"

typedef struct hash_entry_str HASH_ENTRY_T;
typedef struct hashtab_str HASHTAB_T;

struct hash_entry_str {
        HASH_ENTRY_T * nullable bucket_prev;
        HASH_ENTRY_T * nullable bucket_next;
        HASH_ENTRY_T * nullable iter_prev;
        HASH_ENTRY_T * nullable iter_next;
        uint64_t hash;
        size_t key_len;
        void * nullable value;
        uint8_t key[];
};

struct hashtab_str {
        HASH_ENTRY_T * nullable * nonnull buckets;
        HASH_ENTRY_T * nullable head;
        size_t bucket_count;
        size_t size;
};

nodiscard uint64_t hash_calculate(const void * restrict nonnull buf,
                                  size_t len);

nodiscard HASHTAB_T * nonnull hash_create(size_t bucket_count);

void hash_destroy(HASHTAB_T * nullable table,
                  void (*nullable value_destroy)(void * nullable value));

nodiscard HASH_ENTRY_T * nullable
hash_find_entry_with_hash(HASHTAB_T * nonnull table,
                          const void * nonnull key,
                          size_t key_len,
                          uint64_t hash);

nodiscard void * nullable hash_get_with_hash(HASHTAB_T * nonnull table,
                                             const void * nonnull key,
                                             size_t key_len,
                                             uint64_t hash);

HASH_ENTRY_T * nonnull hash_set_with_hash(HASHTAB_T * nonnull table,
                                          const void * nonnull key,
                                          size_t key_len,
                                          uint64_t hash,
                                          void * nullable value);

bool hash_delete_with_hash(
    HASHTAB_T * nonnull table,
    const void * nonnull key,
    size_t key_len,
    uint64_t hash,
    void (*nullable value_destroy)(void * nullable value));

nodiscard HASH_ENTRY_T * nullable hash_find_entry(HASHTAB_T * nonnull table,
                                                  const void * nonnull key,
                                                  size_t key_len);

nodiscard void * nullable hash_get(HASHTAB_T * nonnull table,
                                   const void * nonnull key,
                                   size_t key_len);

HASH_ENTRY_T * nonnull hash_set(HASHTAB_T * nonnull table,
                                const void * nonnull key,
                                size_t key_len,
                                void * nullable value);

bool hash_delete(HASHTAB_T * nonnull table,
                 const void * nonnull key,
                 size_t key_len,
                 void (*nullable value_destroy)(void * nullable value));

nodiscard HASH_ENTRY_T * nullable hash_next(HASH_ENTRY_T * nonnull entry);

#endif /* SH_HASH_H */
