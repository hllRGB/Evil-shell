/* vim: set ts=8 sw=8 nu rnu sts=8 et: */
#include "evilgeneral.h"

#include "hash.h"
#include "libs/memm.h"
#include "rapidhash.h"

static void hash_abort(const char * nonnull message) {
        fputs(message, stderr);
        fputc('\n', stderr);
        abort();
}

static void hash_rehash(HASHTAB_T * table) {
        HASH_ENTRY_T * e = table->head;
        while (e) {
                HASH_ENTRY_T * next = e->entry_next;

                size_t idx = hash_calculate(e->key, e->keylen)
                             & (table->bucket_num - 1);
                // bucket_num必须为2的幂次

                e->prev = NULL;
                e->next = new_buckets[idx];
                if (e->next)
                        e->next->prev = e;
                new_buckets[idx] = e;

                e = next;
        }
}

// ----------------------------------------------------------------------------------------------------

nodiscard uint64_t hash_calculate(const void * restrict nonnull buf,
                                  size_t len) {
        return rapidhashNano(buf, len);
}

nodiscard HASHTAB_T * nonnull hash_tab_create(size_t bucket_num) {
        HASHTAB_T * nonnull table = xmalloc(sizeof(HASHTAB_T *));
        table->bucket_num         = bucket_num;
        table->buckets            = xcalloc(bucket_num, sizeof(HASH_ENTRY_T));
        table->head               = nullptr;
        return table;
}
SUCCESS_T hash_tab_resize(HASHTAB_T * nonnull table, size_t bucket_num) {
        // buckets直接xcalloc一个新的(旧的xfree)，或者考虑realloc后memset,然后拿着table->buckets直接去找rehash.
}
void hash_tab_destroy(HASHTAB_T * nonnull table) {
        // 全给它xfree了
}
void hash_tab_clear(HASHTAB_T * nonnull table) {
        // 从head遍历总链表，entries全xfree了，buckets全置空。考虑memset 0 或者
        // free后xcalloc. head置空，bucket_num=0
}

nodiscard void * nullable hash_read(HASHTAB_T * nonnull table,
                                    const char * nonnull key) {
        // 计算哈希，取索引，找到确切唯一entry,返回找到:0,没找到1.如果未能找到entry不写值.如果找到entry往指针里写值。调用者需要传指针的指针。
}
SUCCESS_T hash_write(HASHTAB_T * nonnull table, const char * nonnull key) {
        // 计算哈希，取索引，找到唯一entry,写入entry->value.找不到就新建再写。
}
SUCCESS_T hash_rm(HASHTAB_T * nonnull table, const char * nonnull key) {
        // 算哈希，取索引，找entry,释放entry并处理好链表。
}

// 下同上，只是不计算哈希，直接用传入的hash来取索引。
nodiscard void * nullable hash_read_hashed(HASHTAB_T * nonnull table,
                                           const char * nonnull key);
SUCCESS_T hash_write_hashed(HASHTAB_T * nonnull table,
                            const char * nonnull key);
SUCCESS_T hash_rm_hashed(HASHTAB_T * nonnull table, const char * nonnull key);
