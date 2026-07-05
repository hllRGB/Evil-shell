// vim: set sw=8 ts=8 et:
/* hash.h — evilshell hash API */
#ifndef SH_HASH_H
#define SH_HASH_H

#include "evilgeneral.h"

#define HASH_DEFAULT_BUCKETS ((size_t)1 << 6) // 1<<6==64

typedef struct hash_entry_str {
        char * nonnull key;
        size_t keylen;
        uint64_t hash;
        void * nullable value;
        struct hash_entry_str * nullable prev;       // 构成桶内链表
        struct hash_entry_str * nullable next;       // 构成桶内链表
        struct hash_entry_str * nullable entry_prev; // 构成总链表
        struct hash_entry_str * nullable entry_next; // 构成总链表
} HASH_ENTRY_T; // 其实是哈希桶和哈希项共用了

typedef struct hashtab_str {
        HASH_ENTRY_T * nullable * nonnull buckets;
        HASH_ENTRY_T * nullable head;
        size_t bucket_num;
        size_t entry_num;
} HASHTAB_T;

nodiscard uint64_t hash_calculate(const void * restrict nonnull buf,
                                  size_t len);

// bucket_num必须为2的幂次
nodiscard HASHTAB_T * nonnull hash_tab_create(size_t bucket_num);
nodiscard SUCCESS_T hash_tab_resize(HASHTAB_T * nonnull table,
                                    size_t bucket_num);
void hash_tab_destroy(HASHTAB_T * nonnull table);
void hash_tab_clear(HASHTAB_T * nonnull table);

nodiscard SUCCESS_T hash_read(HASHTAB_T * nonnull table,
                              const char * nonnull key,
                              size_t keylen,
                              void * nullable * nonnull result);
nodiscard SUCCESS_T hash_write(HASHTAB_T * nonnull table,
                               const char * nonnull key,
                               size_t keylen,
                               void * nullable value);
nodiscard SUCCESS_T hash_rm(HASHTAB_T * nonnull table,
                            const char * nonnull key,
                            size_t keylen);

nodiscard SUCCESS_T hash_read_hashed(HASHTAB_T * nonnull table,
                                     uint64_t hash,
                                     const char * nonnull key,
                                     size_t keylen,
                                     void * nullable * nonnull result);
nodiscard SUCCESS_T hash_write_hashed(HASHTAB_T * nonnull table,
                                      uint64_t hash,
                                      const char * nonnull key,
                                      size_t keylen,
                                      void * nullable value);
nodiscard SUCCESS_T hash_rm_hashed(HASHTAB_T * nonnull table,
                                   uint64_t hash,
                                   const char * nonnull key,
                                   size_t keylen);

#endif /* SH_HASH_H */
