/* vim: set ts=8 sw=8 nu rnu sts=8 et: */
// Evil shell 哈希操作
#include "evilgeneral.h"

#include "hash.h"
#include "libs/memm.h"
#include "rapidhash.h"

static void hash_abort(const char * nonnull message) {
        fputs(message, stderr);
        fputc('\n', stderr);
        abort();
}

static void hash_rehash(HASHTAB_T * nonnull table) {
        HASH_ENTRY_T * e = table->head;
        while (e) {
                HASH_ENTRY_T * next = e->entry_next;

                size_t idx = e->hash & (table->bucket_num - 1);
                // bucket_num必须为2的幂次

                e->prev = nullptr;
                e->next = table->buckets[idx];
                if (e->next)
                        e->next->prev = e;
                table->buckets[idx] = e;

                e = next;
        }
}

static bool hash_bucket_num_valid(size_t bucket_num) {
        return bucket_num && ((bucket_num & (bucket_num - 1)) == 0);
}

static size_t hash_bucket_index(HASHTAB_T * nonnull table, uint64_t hash) {
        return hash & (table->bucket_num - 1);
}

static HASH_ENTRY_T * nullable hash_find_entry_hashed(HASHTAB_T * nonnull table,
                                                      uint64_t hash,
                                                      const char * nonnull key,
                                                      size_t keylen) {
        HASH_ENTRY_T * nullable entry
            = table->buckets[hash_bucket_index(table, hash)];
        while (entry) {
                if (entry->hash == hash && entry->keylen == keylen
                    && memcmp(entry->key, key, keylen) == 0) {
                        return entry;
                }
                entry = entry->next;
        }
        return nullptr;
}

static HASH_ENTRY_T * nonnull hash_entry_new(uint64_t hash,
                                             const char * nonnull key,
                                             size_t keylen,
                                             void * nullable value) {
        if (keylen == SIZE_MAX) {
                hash_abort("hash key too large");
        }

        HASH_ENTRY_T * nonnull entry = xmalloc(sizeof(HASH_ENTRY_T));
        entry->key                   = xmalloc(keylen + 1);
        memcpy(entry->key, key, keylen);
        entry->key[keylen] = '\0';

        entry->keylen     = keylen;
        entry->hash       = hash;
        entry->value      = value;
        entry->prev       = nullptr;
        entry->next       = nullptr;
        entry->entry_prev = nullptr;
        entry->entry_next = nullptr;
        return entry;
}

static void hash_link_entry(HASHTAB_T * nonnull table,
                            HASH_ENTRY_T * nonnull entry) {
        size_t index = hash_bucket_index(table, entry->hash);

        entry->prev = nullptr;
        entry->next = table->buckets[index];
        if (entry->next) {
                entry->next->prev = entry;
        }
        table->buckets[index] = entry;

        entry->entry_prev = nullptr;
        entry->entry_next = table->head;
        if (entry->entry_next) {
                entry->entry_next->entry_prev = entry;
        }
        table->head = entry;
        table->entry_num++;
}

static void hash_unlink_entry(HASHTAB_T * nonnull table,
                              HASH_ENTRY_T * nonnull entry) {
        size_t index = hash_bucket_index(table, entry->hash);

        if (entry->prev) {
                entry->prev->next = entry->next;
        } else {
                table->buckets[index] = entry->next;
        }
        if (entry->next) {
                entry->next->prev = entry->prev;
        }

        if (entry->entry_prev) {
                entry->entry_prev->entry_next = entry->entry_next;
        } else {
                table->head = entry->entry_next;
        }
        if (entry->entry_next) {
                entry->entry_next->entry_prev = entry->entry_prev;
        }
        table->entry_num--;
}

static void hash_entry_free(HASH_ENTRY_T * nonnull entry) {
        xfree(entry->key);
        xfree(entry);
}

// ----------------------------------------------------------------------------------------------------

nodiscard uint64_t hash_calculate(const void * restrict nonnull buf,
                                  size_t len) {
        return rapidhashNano(buf, len);
}

nodiscard HASHTAB_T * nonnull hash_tab_create(size_t bucket_num) {
        if (!hash_bucket_num_valid(bucket_num)) {
                hash_abort("hash bucket_num must be power of two");
        }

        HASHTAB_T * nonnull table = xmalloc(sizeof(HASHTAB_T));
        table->buckets    = xcalloc(bucket_num, sizeof(*table->buckets));
        table->head       = nullptr;
        table->bucket_num = bucket_num;
        table->entry_num  = 0;
        return table;
}

nodiscard SUCCESS_T hash_tab_resize(HASHTAB_T * nonnull table,
                                    size_t bucket_num) {
        // Resize buckets, discard old bucket links, rebuild all bucket links.
        if (!hash_bucket_num_valid(bucket_num)) {
                return FAIL;
        }

        xfree(table->buckets);
        table->buckets    = xcalloc(bucket_num, sizeof(*table->buckets));
        table->bucket_num = bucket_num;

        // Discard buckets and prev/next links; rebuild from entry_next.
        hash_rehash(table);
        return SUCCESS;
}

void hash_tab_destroy(HASHTAB_T * nonnull table) {
        // Free all entries, buckets, and table itself.
        hash_tab_clear(table);
        xfree(table->buckets);
        xfree(table);
}

void hash_tab_clear(HASHTAB_T * nonnull table) {
        // Free all entries, clear buckets, reset head and entry count.
        HASH_ENTRY_T * nullable entry = table->head;
        while (entry) {
                HASH_ENTRY_T * nullable next = entry->entry_next;
                hash_entry_free(entry);
                entry = next;
        }
        memset(table->buckets, 0, table->bucket_num * sizeof(*table->buckets));
        table->head      = nullptr;
        table->entry_num = 0;
}

nodiscard SUCCESS_T hash_read(HASHTAB_T * nonnull table,
                              const char * nonnull key,
                              size_t keylen,
                              void * nullable * nonnull result) {
        // Hash key, find exact entry, return SUCCESS and write result if found.
        return hash_read_hashed(
            table, hash_calculate(key, keylen), key, keylen, result);
}

nodiscard SUCCESS_T hash_write(HASHTAB_T * nonnull table,
                               const char * nonnull key,
                               size_t keylen,
                               void * nullable value) {
        // Hash key, find exact entry, update value; create entry if absent.
        return hash_write_hashed(
            table, hash_calculate(key, keylen), key, keylen, value);
}

nodiscard SUCCESS_T hash_rm(HASHTAB_T * nonnull table,
                            const char * nonnull key,
                            size_t keylen) {
        // Hash key, find exact entry, unlink and free it.
        return hash_rm_hashed(table, hash_calculate(key, keylen), key, keylen);
}

// Same as above, but use caller-provided hash.
nodiscard SUCCESS_T hash_read_hashed(HASHTAB_T * nonnull table,
                                     uint64_t hash,
                                     const char * nonnull key,
                                     size_t keylen,
                                     void * nullable * nonnull result) {
        HASH_ENTRY_T * nullable entry
            = hash_find_entry_hashed(table, hash, key, keylen);
        if (!entry) {
                return FAIL;
        }
        *result = entry->value;
        return SUCCESS;
}

nodiscard SUCCESS_T hash_write_hashed(HASHTAB_T * nonnull table,
                                      uint64_t hash,
                                      const char * nonnull key,
                                      size_t keylen,
                                      void * nullable value) {
        HASH_ENTRY_T * nullable entry
            = hash_find_entry_hashed(table, hash, key, keylen);
        if (entry) {
                entry->value = value;
                return SUCCESS;
        }

        hash_link_entry(table, hash_entry_new(hash, key, keylen, value));
        return SUCCESS;
}

nodiscard SUCCESS_T hash_rm_hashed(HASHTAB_T * nonnull table,
                                   uint64_t hash,
                                   const char * nonnull key,
                                   size_t keylen) {
        HASH_ENTRY_T * nullable entry
            = hash_find_entry_hashed(table, hash, key, keylen);
        if (!entry) {
                return FAIL;
        }

        hash_unlink_entry(table, entry);
        hash_entry_free(entry);
        return SUCCESS;
}
