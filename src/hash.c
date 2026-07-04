/* vim: set ts=8 sw=8 nu rnu sts=8 et: */
#include "include/evilgeneral.h"

#include "hash.h"
#include "libs/memm.h"
#include "rapidhash.h"

#define HASH_DEFAULT_BUCKETS ((size_t)64)

static void hash_abort(const char * nonnull message) {
        fputs(message, stderr);
        fputc('\n', stderr);
        abort();
}

nodiscard static size_t hash_next_power_of_two(size_t value) {
        if (value == 0)
                return HASH_DEFAULT_BUCKETS;
        if (value > (SIZE_MAX >> 1) + 1)
                hash_abort("hash bucket count overflow");
        value--;
        for (size_t shift = 1; shift < sizeof(value) * CHAR_BIT; shift <<= 1)
                value |= value >> shift;
        return value + 1;
}

nodiscard static size_t hash_bucket_index(const HASHTAB_T * nonnull table,
                                          uint64_t hash) {
        return (size_t)hash & (table->bucket_count - 1);
}

static bool entry_key_equals(const HASH_ENTRY_T * nonnull entry,
                             const void * nonnull key,
                             size_t key_len) {
        return entry->key_len == key_len
               && memcmp(entry->key, key, key_len) == 0;
}

static void bucket_insert(HASHTAB_T * nonnull table,
                          HASH_ENTRY_T * nonnull entry) {
        size_t index = hash_bucket_index(table, entry->hash);

        entry->bucket_next = table->buckets[index];
        if (entry->bucket_next != NULL)
                entry->bucket_next->bucket_prev = entry;
        table->buckets[index] = entry;
}

static void iter_insert(HASHTAB_T * nonnull table,
                        HASH_ENTRY_T * nonnull entry) {
        entry->iter_next = table->head;
        if (entry->iter_next != NULL)
                entry->iter_next->iter_prev = entry;
        table->head = entry;
}

static void bucket_remove(HASHTAB_T * nonnull table,
                          HASH_ENTRY_T * nonnull entry) {
        size_t index = hash_bucket_index(table, entry->hash);

        if (entry->bucket_prev != NULL)
                entry->bucket_prev->bucket_next = entry->bucket_next;
        else
                table->buckets[index] = entry->bucket_next;
        if (entry->bucket_next != NULL)
                entry->bucket_next->bucket_prev = entry->bucket_prev;
}

static void iter_remove(HASHTAB_T * nonnull table,
                        HASH_ENTRY_T * nonnull entry) {
        if (entry->iter_prev != NULL)
                entry->iter_prev->iter_next = entry->iter_next;
        else
                table->head = entry->iter_next;
        if (entry->iter_next != NULL)
                entry->iter_next->iter_prev = entry->iter_prev;
}

// ------------------------------------------------------------------------------------------------

nodiscard uint64_t hash_calculate(const void * restrict nonnull buf,
                                  size_t len) {
        return rapidhashNano(buf, len); // 语义封装。
}

nodiscard HASHTAB_T * nonnull hash_create(size_t bucket_count) {
        HASHTAB_T * table = xmalloc(sizeof(*table));

        table->bucket_count = hash_next_power_of_two(bucket_count);
        table->buckets = xcalloc(table->bucket_count, sizeof(*table->buckets));
        table->head    = NULL;
        table->size    = 0;
        return table;
}

nodiscard HASH_ENTRY_T * nullable
hash_find_entry_with_hash(HASHTAB_T * nonnull table,
                          const void * nonnull key,
                          size_t key_len,
                          uint64_t hash) {
        size_t index = hash_bucket_index(table, hash);

        for (HASH_ENTRY_T * entry = table->buckets[index]; entry != NULL;
             entry                = entry->bucket_next) {
                if (entry->hash == hash
                    && entry_key_equals(entry, key, key_len))
                        return entry;
        }
        return NULL;
}

nodiscard HASH_ENTRY_T * nullable hash_find_entry(HASHTAB_T * nonnull table,
                                                  const void * nonnull key,
                                                  size_t key_len) {
        return hash_find_entry_with_hash(
            table, key, key_len, hash_calculate(key, key_len));
}

nodiscard void * nullable hash_get_with_hash(HASHTAB_T * nonnull table,
                                             const void * nonnull key,
                                             size_t key_len,
                                             uint64_t hash) {
        HASH_ENTRY_T * entry
            = hash_find_entry_with_hash(table, key, key_len, hash);

        return entry == NULL ? NULL : entry->value;
}

nodiscard void * nullable hash_get(HASHTAB_T * nonnull table,
                                   const void * nonnull key,
                                   size_t key_len) {
        return hash_get_with_hash(
            table, key, key_len, hash_calculate(key, key_len));
}

HASH_ENTRY_T * nonnull hash_set_with_hash(HASHTAB_T * nonnull table,
                                          const void * nonnull key,
                                          size_t key_len,
                                          uint64_t hash,
                                          void * nullable value) {
        HASH_ENTRY_T * entry
            = hash_find_entry_with_hash(table, key, key_len, hash);

        if (entry != NULL) {
                entry->value = value;
                return entry;
        }
        if (key_len > SIZE_MAX - sizeof(*entry))
                hash_abort("hash entry size overflow");
        entry = xmalloc(sizeof(*entry) + key_len);
        memset(entry, 0, sizeof(*entry));
        entry->hash    = hash;
        entry->key_len = key_len;
        entry->value   = value;
        memcpy(entry->key, key, key_len);
        bucket_insert(table, entry);
        iter_insert(table, entry);
        table->size++;
        return entry;
}

HASH_ENTRY_T * nonnull hash_set(HASHTAB_T * nonnull table,
                                const void * nonnull key,
                                size_t key_len,
                                void * nullable value) {
        return hash_set_with_hash(
            table, key, key_len, hash_calculate(key, key_len), value);
}

bool hash_delete_with_hash(
    HASHTAB_T * nonnull table,
    const void * nonnull key,
    size_t key_len,
    uint64_t hash,
    void (*nullable value_destroy)(void * nullable value)) {
        HASH_ENTRY_T * entry
            = hash_find_entry_with_hash(table, key, key_len, hash);

        if (entry == NULL)
                return false;
        bucket_remove(table, entry);
        iter_remove(table, entry);
        if (value_destroy != NULL)
                value_destroy(entry->value);
        xfree(entry);
        table->size--;
        return true;
}

bool hash_delete(HASHTAB_T * nonnull table,
                 const void * nonnull key,
                 size_t key_len,
                 void (*nullable value_destroy)(void * nullable value)) {
        return hash_delete_with_hash(
            table, key, key_len, hash_calculate(key, key_len), value_destroy);
}

void hash_destroy(HASHTAB_T * nullable table,
                  void (*nullable value_destroy)(void * nullable value)) {
        HASH_ENTRY_T * entry;

        if (table == NULL)
                return;
        entry = table->head;
        while (entry != NULL) {
                HASH_ENTRY_T * next = entry->iter_next;
                if (value_destroy != NULL)
                        value_destroy(entry->value);
                xfree(entry);
                entry = next;
        }
        xfree(table->buckets);
        xfree(table);
}

nodiscard HASH_ENTRY_T * nullable hash_next(HASH_ENTRY_T * nonnull entry) {
        return entry->iter_next;
}
