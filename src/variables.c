/* vim: set ts=8 sw=8 nu rnu sts=8 et: */
#include "include/evilgeneral.h"

#include "hash.h"
#include "libs/memm.h"

#include "variables.h"

#define VAR_SCOPE_BUCKETS UINT64_C(64)
#define VAR_ARRAY_BUCKETS UINT64_C(64)
#define VAR_OWN_REPLACE ((BITMASK32_T)VAR_OWN_TAKE_NEW | (BITMASK32_T)VAR_OWN_FREE_OLD)
#define VAR_OWN_KNOWN_FLAGS (VAR_OWN_REPLACE | (BITMASK32_T)VAR_OWN_RETURN_OLD)
#define VAR_ARRAY_ERR ((VAR_ARRAY_T * nullable) VAR_PTR_ERR)

static void s_var_abort(const char * nonnull message) {
        fputs(message, stderr);
        fputc('\n', stderr);
        abort();
}

static bool s_own_flags_take_new(BITMASK32_T flags) {
        return (flags & (BITMASK32_T)VAR_OWN_TAKE_NEW) != 0;
}

static bool s_own_flags_free_old(BITMASK32_T flags) {
        return (flags & (BITMASK32_T)VAR_OWN_FREE_OLD) != 0;
}

static bool s_own_flags_return_old(BITMASK32_T flags) {
        return (flags & (BITMASK32_T)VAR_OWN_RETURN_OLD) != 0;
}

static bool s_own_flags_valid(BITMASK32_T flags) {
        if ((flags & ~VAR_OWN_KNOWN_FLAGS) != 0)
                return false;
        return !(s_own_flags_free_old(flags) && s_own_flags_return_old(flags));
}

static bool s_var_is_readonly(const VAR_T * nonnull var) {
        return (var->attr & (BITMASK32_T)VAR_ATTR_READONLY) != 0;
}

static char * nonnull s_memdup_string(const char * nonnull string, size_t len) {
        char * copy;

        if (len == SIZE_MAX)
                s_var_abort("string length overflow");
        copy = xmalloc(len + (size_t)1);
        memcpy(copy, string, len);
        copy[len] = '\0';
        return copy;
}

static bool s_name_first_char(unsigned char ch) {
        return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_';
}

static bool s_name_char(unsigned char ch) {
        return s_name_first_char(ch) || (ch >= '0' && ch <= '9');
}

static bool s_value_kind_is_array(VAR_VALUE_KIND_T kind) {
        return kind == VAR_VALUE_INDEXED_ARRAY || kind == VAR_VALUE_ASSOC_ARRAY;
}

static VAR_VALUE_KIND_T s_array_value_kind(VAR_ARRAY_KIND_T kind) {
        return kind == VAR_ARRAY_ASSOC ? VAR_VALUE_ASSOC_ARRAY : VAR_VALUE_INDEXED_ARRAY;
}

static VAR_ARRAY_T * nullable s_value_array(VAR_VALUE_T * nullable value) {
        if (value == NULL)
                return NULL;
        if (!s_value_kind_is_array(value->kind))
                return NULL;
        return value->as.array;
}

static VAR_VALUE_T * nullable s_own_old_result(VAR_VALUE_T * nullable old, BITMASK32_T flags) {
        if (old == NULL)
                return NULL;
        if (s_own_flags_return_old(flags))
                return old;
        if (s_own_flags_free_old(flags) || !s_own_flags_return_old(flags))
                var_value_free(old);
        return NULL;
}

static VAR_VALUE_T * nullable s_own_new_value(VAR_VALUE_T * nullable value, BITMASK32_T flags) {
        if (value == NULL)
                return NULL;
        if (s_own_flags_take_new(flags))
                return value;
        return var_value_clone(value);
}

static VAR_VALUE_T * nullable s_own_callback_result(VAR_VALUE_T * nullable result,
                                                    BITMASK32_T flags) {
        if (var_value_is_err(result))
                return VAR_VALUE_ERR;
        return s_own_old_result(result, flags);
}

static VAR_ARRAY_T * nonnull s_array_clone(const VAR_ARRAY_T * nonnull src) {
        VAR_ARRAY_T * dst = var_array_create(src->kind);

        dst->call      = src->call;
        dst->call_ops  = src->call_ops;
        dst->call_data = src->call_data;
        for (HASH_ENTRY_T * entry = src->values->head; entry != NULL; entry = hash_next(entry)) {
                VAR_VALUE_T * src_value = entry->value;
                VAR_VALUE_T * dst_value;

                if (src_value == NULL)
                        continue;
                dst_value = var_value_clone(src_value);
                if (var_value_is_err(dst_value))
                        s_var_abort("array clone failed");
                hash_set(dst->values, entry->key, entry->key_len, dst_value);
        }
        return dst;
}

static VAR_VALUE_T * nonnull s_value_create_array(VAR_ARRAY_KIND_T kind) {
        VAR_VALUE_T * value = xmalloc(sizeof(*value));

        value->kind     = s_array_value_kind(kind);
        value->as.array = var_array_create(kind);
        return value;
}

static void s_array_value_destroy(void * nullable value) { var_value_free(value); }

static VAR_VALUE_T * nullable s_array_delete_value(VAR_ARRAY_T * nonnull array,
                                                   const VAR_SUBSCRIPT_T * nonnull subscript,
                                                   BITMASK32_T flags) {
        VAR_VALUE_T * old;

        if (!s_own_flags_valid(flags))
                return VAR_VALUE_ERR;
        old = hash_get(array->values, subscript->text, subscript->len);
        if (old == NULL)
                return NULL;
        if (s_own_flags_return_old(flags)) {
                hash_delete(array->values, subscript->text, subscript->len, NULL);
                return old;
        }
        hash_delete(array->values, subscript->text, subscript->len, s_array_value_destroy);
        return NULL;
}

static VAR_VALUE_T * nullable s_array_set_value(VAR_ARRAY_T * nonnull array,
                                                const VAR_SUBSCRIPT_T * nonnull subscript,
                                                VAR_VALUE_T * nullable value,
                                                BITMASK32_T flags) {
        VAR_VALUE_T * new_value;
        VAR_VALUE_T * old;

        if (!s_own_flags_valid(flags))
                return VAR_VALUE_ERR;
        if (value == NULL)
                return s_array_delete_value(array, subscript, flags);
        new_value = s_own_new_value(value, flags);
        if (var_value_is_err(new_value))
                return VAR_VALUE_ERR;
        old = hash_get(array->values, subscript->text, subscript->len);
        if (old == new_value)
                return NULL;
        if (old != NULL) {
                if (s_own_flags_return_old(flags))
                        hash_delete(array->values, subscript->text, subscript->len, NULL);
                else
                        hash_delete(
                            array->values, subscript->text, subscript->len, s_array_value_destroy);
        }
        hash_set(array->values, subscript->text, subscript->len, new_value);
        return s_own_flags_return_old(flags) ? old : NULL;
}

static void s_scope_entry_destroy(void * nullable value) {
        VAR_SCOPE_ENTRY_T * entry = value;

        if (entry == NULL)
                return;
        if (entry->kind == VAR_SCOPE_ENTRY_VAR)
                var_destroy(entry->var);
        xfree(entry);
}

static VAR_SCOPE_ENTRY_T * nonnull s_scope_entry_new(VAR_SCOPE_ENTRY_KIND_T kind,
                                                     VAR_T * nullable var) {
        VAR_SCOPE_ENTRY_T * entry = xmalloc(sizeof(*entry));

        entry->kind = kind;
        entry->var  = var;
        return entry;
}

static VAR_SCOPE_ENTRY_T * nullable s_scope_get_entry(VAR_SCOPE_T * nonnull scope,
                                                      const char * nonnull name,
                                                      size_t name_len) {
        return hash_get(scope->vars, name, name_len);
}

static VAR_T * nullable s_scope_entry_to_var(VAR_SCOPE_ENTRY_T * nullable entry) {
        if (entry == NULL || entry->kind == VAR_SCOPE_ENTRY_TOMBSTONE)
                return NULL;
        return entry->var;
}

static void s_scope_set_entry(VAR_SCOPE_T * nonnull scope,
                              const char * nonnull name,
                              size_t name_len,
                              VAR_SCOPE_ENTRY_T * nonnull entry) {
        hash_delete(scope->vars, name, name_len, s_scope_entry_destroy);
        hash_set(scope->vars, name, name_len, entry);
}

static VAR_T * nonnull s_scope_create_var(VAR_SCOPE_T * nonnull scope,
                                          const char * nonnull name,
                                          size_t name_len,
                                          BITMASK32_T attr) {
        VAR_T * var               = var_create(name, name_len, attr);
        VAR_SCOPE_ENTRY_T * entry = s_scope_entry_new(VAR_SCOPE_ENTRY_VAR, var);

        s_scope_set_entry(scope, name, name_len, entry);
        return var;
}

static VAR_T * nullable s_scope_get_or_create_var(VAR_SCOPE_T * nonnull scope,
                                                  const char * nonnull name,
                                                  size_t name_len,
                                                  BITMASK32_T attr) {
        VAR_SCOPE_ENTRY_T * entry = s_scope_get_entry(scope, name, name_len);
        VAR_T * var               = s_scope_entry_to_var(entry);

        if (var != NULL)
                return var;
        if (entry != NULL && entry->kind == VAR_SCOPE_ENTRY_VAR)
                return VAR_ERR;
        return s_scope_create_var(scope, name, name_len, attr);
}

static VAR_T * nullable s_scope_detach_var(VAR_SCOPE_T * nonnull scope,
                                           const char * nonnull name,
                                           size_t name_len) {
        VAR_SCOPE_ENTRY_T * entry = s_scope_get_entry(scope, name, name_len);
        VAR_T * var               = s_scope_entry_to_var(entry);

        if (var == NULL)
                return VAR_ERR;
        entry->var = NULL;
        hash_delete(scope->vars, name, name_len, xfree);
        return var;
}

static SUCCESS_T s_scope_attach_var(VAR_SCOPE_T * nonnull scope,
                                    const char * nonnull name,
                                    size_t name_len,
                                    VAR_T * nonnull var) {
        if (s_scope_get_entry(scope, name, name_len) != NULL)
                return FAIL;
        hash_set(scope->vars, name, name_len, s_scope_entry_new(VAR_SCOPE_ENTRY_VAR, var));
        return SUCCESS;
}

static VAR_ARRAY_KIND_T s_var_default_array_kind(const VAR_T * nonnull var) {
        return (var->attr & (BITMASK32_T)VAR_ATTR_ASSOC) != 0 ? VAR_ARRAY_ASSOC : VAR_ARRAY_INDEXED;
}

static VAR_ARRAY_T * nullable s_var_array_for_write(VAR_T * nonnull var) {
        VAR_VALUE_T * stored = var_get(var);
        VAR_VALUE_T * created;
        VAR_VALUE_T * old;

        if (var_value_is_err(stored))
                return VAR_ARRAY_ERR;
        if (stored != NULL) {
                if (!s_value_kind_is_array(stored->kind))
                        return VAR_ARRAY_ERR;
                return stored->as.array;
        }
        created = s_value_create_array(s_var_default_array_kind(var));
        old     = var_set(var, created, VAR_OWN_REPLACE);
        if (var_value_is_err(old)) {
                var_value_free(created);
                return VAR_ARRAY_ERR;
        }
        return created->as.array;
}

void var_value_init_unset(VAR_VALUE_T * nonnull value) { value->kind = VAR_VALUE_UNSET; }

SUCCESS_T
var_value_init_string(VAR_VALUE_T * nonnull value, const char * nonnull string) {
        return var_value_init_string_len(value, string, strlen(string));
}

SUCCESS_T
var_value_init_string_len(VAR_VALUE_T * nonnull value, const char * nonnull string, size_t len) {
        value->kind           = VAR_VALUE_STRING;
        value->as.string.data = s_memdup_string(string, len);
        value->as.string.len  = len;
        return SUCCESS;
}

void var_value_init_int(VAR_VALUE_T * nonnull value, SH_ARITH_T integer) {
        value->kind       = VAR_VALUE_INT;
        value->as.integer = integer;
}

SUCCESS_T
var_value_copy(VAR_VALUE_T * nonnull dst, const VAR_VALUE_T * nonnull src) {
        switch (src->kind) {
        case VAR_VALUE_UNSET:
                var_value_init_unset(dst);
                return SUCCESS;
        case VAR_VALUE_STRING:
                return var_value_init_string_len(dst, src->as.string.data, src->as.string.len);
        case VAR_VALUE_INT:
                var_value_init_int(dst, src->as.integer);
                return SUCCESS;
        case VAR_VALUE_INDEXED_ARRAY:
        case VAR_VALUE_ASSOC_ARRAY:
                dst->kind     = src->kind;
                dst->as.array = s_array_clone(src->as.array);
                return SUCCESS;
        }
        return FAIL;
}

nodiscard VAR_VALUE_T * nullable var_value_clone(const VAR_VALUE_T * nonnull src) {
        VAR_VALUE_T * dst = xmalloc(sizeof(*dst));

        if (var_value_copy(dst, src) != SUCCESS) {
                xfree(dst);
                return VAR_VALUE_ERR;
        }
        return dst;
}

void var_value_destroy(VAR_VALUE_T * nonnull value) {
        switch (value->kind) {
        case VAR_VALUE_STRING:
                xfree(value->as.string.data);
                break;
        case VAR_VALUE_INDEXED_ARRAY:
        case VAR_VALUE_ASSOC_ARRAY:
                var_array_destroy(value->as.array);
                break;
        case VAR_VALUE_UNSET:
        case VAR_VALUE_INT:
                break;
        }
        var_value_init_unset(value);
}

void var_value_free(VAR_VALUE_T * nullable value) {
        if (value == NULL || var_value_is_err(value))
                return;
        var_value_destroy(value);
        xfree(value);
}

nodiscard VAR_ARRAY_T * nonnull var_array_create(VAR_ARRAY_KIND_T kind) {
        VAR_ARRAY_T * array = xmalloc(sizeof(*array));

        array->kind      = kind;
        array->values    = hash_create(VAR_ARRAY_BUCKETS);
        array->call      = false;
        array->call_ops  = NULL;
        array->call_data = NULL;
        return array;
}

void var_array_destroy(VAR_ARRAY_T * nullable array) {
        if (array == NULL)
                return;
        hash_destroy(array->values, s_array_value_destroy);
        xfree(array);
}

void var_array_set_call(VAR_ARRAY_T * nonnull array,
                        const VAR_ARRAY_CALL_OPS_T * nullable ops,
                        void * nullable data) {
        array->call      = ops != NULL;
        array->call_ops  = ops;
        array->call_data = data;
}

nodiscard bool var_name_is_valid(const char * nonnull name) {
        const unsigned char * cursor = (const unsigned char *)name;

        if (!s_name_first_char(*cursor))
                return false;
        for (cursor++; *cursor != '\0'; cursor++) {
                if (!s_name_char(*cursor))
                        return false;
        }
        return true;
}

nodiscard VAR_SCOPE_T * nonnull var_scope_create(VAR_SCOPE_KIND_T kind,
                                                 VAR_SCOPE_T * nullable next) {
        VAR_SCOPE_T * scope = xmalloc(sizeof(*scope));

        scope->kind = kind;
        scope->next = next;
        scope->vars = hash_create(VAR_SCOPE_BUCKETS);
        return scope;
}

void var_scope_destroy(VAR_SCOPE_T * nullable scope) {
        if (scope == NULL)
                return;
        hash_destroy(scope->vars, s_scope_entry_destroy);
        xfree(scope);
}

nodiscard VAR_LOOKUP_T var_scope_get(VAR_SCOPE_T * nullable start, const char * nonnull name) {
        size_t name_len = strlen(name);

        for (VAR_SCOPE_T * scope = start; scope != NULL; scope = scope->next) {
                VAR_SCOPE_ENTRY_T * entry = s_scope_get_entry(scope, name, name_len);
                VAR_T * var               = s_scope_entry_to_var(entry);

                if (var != NULL)
                        return (VAR_LOOKUP_T){.kind = VAR_LOOKUP_VAR, .scope = scope, .var = var};
                if (entry != NULL && entry->kind == VAR_SCOPE_ENTRY_TOMBSTONE)
                        return (VAR_LOOKUP_T){
                            .kind = VAR_LOOKUP_TOMBSTONE, .scope = scope, .var = NULL};
        }
        return (VAR_LOOKUP_T){.kind = VAR_LOOKUP_NOT_FOUND, .scope = NULL, .var = NULL};
}

nodiscard VAR_LOOKUP_T var_scope_get_in(VAR_SCOPE_T * nonnull scope, const char * nonnull name) {
        size_t name_len           = strlen(name);
        VAR_SCOPE_ENTRY_T * entry = s_scope_get_entry(scope, name, name_len);
        VAR_T * var               = s_scope_entry_to_var(entry);

        if (var != NULL)
                return (VAR_LOOKUP_T){.kind = VAR_LOOKUP_VAR, .scope = scope, .var = var};
        if (entry != NULL && entry->kind == VAR_SCOPE_ENTRY_TOMBSTONE)
                return (VAR_LOOKUP_T){.kind = VAR_LOOKUP_TOMBSTONE, .scope = scope, .var = NULL};
        return (VAR_LOOKUP_T){.kind = VAR_LOOKUP_NOT_FOUND, .scope = scope, .var = NULL};
}

nodiscard VAR_T * nullable var_scope_set(VAR_SCOPE_T * nonnull start,
                                         VAR_SCOPE_T * nonnull fallback,
                                         const char * nonnull name,
                                         VAR_VALUE_T * nullable value,
                                         BITMASK32_T flags,
                                         BITMASK32_T create_attr,
                                         VAR_VALUE_T * nullable * nullable old_out) {
        size_t name_len;
        VAR_LOOKUP_T found;
        VAR_T * var;
        VAR_VALUE_T * old;

        if (old_out != NULL)
                *old_out = NULL;
        if (!var_name_is_valid(name) || !s_own_flags_valid(flags))
                return VAR_ERR;
        if (s_own_flags_return_old(flags) && old_out == NULL)
                return VAR_ERR;
        name_len = strlen(name);
        found    = var_scope_get(start, name);
        if (found.kind == VAR_LOOKUP_NOT_FOUND)
                return var_scope_set_in(fallback, name, value, flags, create_attr, old_out);
        if (found.scope == NULL)
                return VAR_ERR;
        var = found.kind == VAR_LOOKUP_VAR
                  ? found.var
                  : s_scope_get_or_create_var(found.scope, name, name_len, create_attr);
        if (var == NULL || var_is_err(var))
                return VAR_ERR;
        old = var_set(var, value, flags);
        if (var_value_is_err(old))
                return VAR_ERR;
        if (old_out != NULL)
                *old_out = old;
        else
                var_value_free(old);
        return var;
}

nodiscard VAR_T * nullable var_scope_set_in(VAR_SCOPE_T * nonnull scope,
                                            const char * nonnull name,
                                            VAR_VALUE_T * nullable value,
                                            BITMASK32_T flags,
                                            BITMASK32_T create_attr,
                                            VAR_VALUE_T * nullable * nullable old_out) {
        size_t name_len;
        VAR_T * var;
        VAR_VALUE_T * old;

        if (old_out != NULL)
                *old_out = NULL;
        if (!var_name_is_valid(name) || !s_own_flags_valid(flags))
                return VAR_ERR;
        if (s_own_flags_return_old(flags) && old_out == NULL)
                return VAR_ERR;
        name_len = strlen(name);
        var      = s_scope_get_or_create_var(scope, name, name_len, create_attr);
        if (var == NULL || var_is_err(var))
                return VAR_ERR;
        old = var_set(var, value, flags);
        if (var_value_is_err(old))
                return VAR_ERR;
        if (old_out != NULL)
                *old_out = old;
        else
                var_value_free(old);
        return var;
}

SUCCESS_T var_scope_delete(VAR_SCOPE_T * nonnull start, const char * nonnull name) {
        VAR_LOOKUP_T found;

        if (!var_name_is_valid(name))
                return FAIL;
        found = var_scope_get(start, name);
        if (found.kind == VAR_LOOKUP_NOT_FOUND || found.kind == VAR_LOOKUP_TOMBSTONE)
                return SUCCESS;
        if (found.scope == NULL)
                return FAIL;
        return var_scope_delete_in(found.scope, name);
}

SUCCESS_T var_scope_delete_in(VAR_SCOPE_T * nonnull scope, const char * nonnull name) {
        size_t name_len;
        VAR_SCOPE_ENTRY_T * entry;

        if (!var_name_is_valid(name))
                return FAIL;
        name_len = strlen(name);
        entry    = s_scope_get_entry(scope, name, name_len);
        if (entry != NULL && entry->kind == VAR_SCOPE_ENTRY_VAR) {
                if (entry->var == NULL)
                        return FAIL;
                if (var_value_is_err(var_delete(entry->var, (BITMASK32_T)VAR_OWN_FREE_OLD)))
                        return FAIL;
        }
        hash_delete(scope->vars, name, name_len, s_scope_entry_destroy);
        hash_set(scope->vars, name, name_len, s_scope_entry_new(VAR_SCOPE_ENTRY_TOMBSTONE, NULL));
        return SUCCESS;
}

SUCCESS_T var_scope_list(VAR_SCOPE_T * nonnull scope,
                         SUCCESS_T (*nonnull callback)(VAR_T * nonnull var, void * nullable data),
                         void * nullable data) {
        for (HASH_ENTRY_T * entry = scope->vars->head; entry != NULL; entry = hash_next(entry)) {
                VAR_T * var = s_scope_entry_to_var(entry->value);

                if (var != NULL && callback(var, data) != SUCCESS)
                        return FAIL;
        }
        return SUCCESS;
}

nodiscard VAR_LOOKUP_T var_find(VAR_SCOPE_T * nullable start, const char * nonnull name) {
        return var_scope_get(start, name);
}

nodiscard VAR_LOOKUP_T var_find_in(VAR_SCOPE_T * nonnull scope, const char * nonnull name) {
        return var_scope_get_in(scope, name);
}

nodiscard VAR_T * nullable var_find_visible(VAR_SCOPE_T * nullable start,
                                            const char * nonnull name) {
        VAR_LOOKUP_T found = var_scope_get(start, name);

        return found.kind == VAR_LOOKUP_VAR ? found.var : NULL;
}

nodiscard VAR_T * nullable var_find_in_scope(VAR_SCOPE_T * nonnull scope,
                                             const char * nonnull name) {
        VAR_LOOKUP_T found = var_scope_get_in(scope, name);

        return found.kind == VAR_LOOKUP_VAR ? found.var : NULL;
}

SUCCESS_T
var_scope_write(VAR_SCOPE_T * nonnull start,
                VAR_SCOPE_T * nonnull fallback,
                const char * nonnull name,
                const VAR_VALUE_T * nonnull value,
                BITMASK32_T create_attr) {
        VAR_VALUE_T * copy = var_value_clone(value);
        VAR_T * var;

        if (var_value_is_err(copy))
                return FAIL;
        var = var_scope_set(start, fallback, name, copy, VAR_OWN_REPLACE, create_attr, NULL);
        if (var_is_err(var)) {
                var_value_free(copy);
                return FAIL;
        }
        return SUCCESS;
}

SUCCESS_T
var_scope_write_in(VAR_SCOPE_T * nonnull scope,
                   const char * nonnull name,
                   const VAR_VALUE_T * nonnull value,
                   BITMASK32_T create_attr) {
        VAR_VALUE_T * copy = var_value_clone(value);
        VAR_T * var;

        if (var_value_is_err(copy))
                return FAIL;
        var = var_scope_set_in(scope, name, copy, VAR_OWN_REPLACE, create_attr, NULL);
        if (var_is_err(var)) {
                var_value_free(copy);
                return FAIL;
        }
        return SUCCESS;
}

SUCCESS_T var_scope_chattr(VAR_SCOPE_T * nonnull start,
                           VAR_SCOPE_T * nonnull fallback,
                           const char * nonnull name,
                           BITMASK32_T set_attr,
                           BITMASK32_T clear_attr,
                           BITMASK32_T create_attr) {
        size_t name_len;
        VAR_LOOKUP_T found;
        VAR_T * var;

        if (!var_name_is_valid(name))
                return FAIL;
        name_len = strlen(name);
        found    = var_scope_get(start, name);
        if (found.kind == VAR_LOOKUP_NOT_FOUND)
                return var_scope_chattr_in(fallback, name, set_attr, clear_attr, create_attr);
        if (found.scope == NULL)
                return FAIL;
        var = found.kind == VAR_LOOKUP_VAR
                  ? found.var
                  : s_scope_get_or_create_var(found.scope, name, name_len, create_attr);
        if (var == NULL || var_is_err(var))
                return FAIL;
        return var_chattr(var, set_attr, clear_attr);
}

SUCCESS_T var_scope_chattr_in(VAR_SCOPE_T * nonnull scope,
                              const char * nonnull name,
                              BITMASK32_T set_attr,
                              BITMASK32_T clear_attr,
                              BITMASK32_T create_attr) {
        size_t name_len;
        VAR_T * var;

        if (!var_name_is_valid(name))
                return FAIL;
        name_len = strlen(name);
        var      = s_scope_get_or_create_var(scope, name, name_len, create_attr);
        if (var == NULL || var_is_err(var))
                return FAIL;
        return var_chattr(var, set_attr, clear_attr);
}

SUCCESS_T var_scope_unset(VAR_SCOPE_T * nonnull start, const char * nonnull name) {
        return var_scope_delete(start, name);
}

SUCCESS_T var_scope_unset_in(VAR_SCOPE_T * nonnull scope, const char * nonnull name) {
        return var_scope_delete_in(scope, name);
}

SUCCESS_T
var_promote(VAR_SCOPE_T * nonnull from, VAR_SCOPE_T * nonnull to, const char * nonnull name) {
        size_t name_len;
        VAR_T * var;

        if (!var_name_is_valid(name))
                return FAIL;
        name_len = strlen(name);
        if (s_scope_get_entry(to, name, name_len) != NULL)
                return FAIL;
        var = s_scope_detach_var(from, name, name_len);
        if (var_is_err(var))
                return FAIL;
        if (s_scope_attach_var(to, name, name_len, var) != SUCCESS) {
                s_scope_attach_var(from, name, name_len, var);
                return FAIL;
        }
        return SUCCESS;
}

nodiscard VAR_T * nonnull var_create(const char * nonnull name, size_t name_len, BITMASK32_T attr) {
        VAR_T * var = xmalloc(sizeof(*var));

        var->name      = s_memdup_string(name, name_len);
        var->name_len  = name_len;
        var->value     = NULL;
        var->attr      = attr;
        var->call      = false;
        var->call_ops  = NULL;
        var->call_data = NULL;
        return var;
}

void var_destroy(VAR_T * nullable var) {
        if (var == NULL || var_is_err(var))
                return;
        var_value_free(var->value);
        xfree(var->name);
        xfree(var);
}

nodiscard VAR_VALUE_T * nullable var_get(VAR_T * nonnull var) {
        if (var->call && var->call_ops != NULL && var->call_ops->get != NULL)
                return var->call_ops->get(var);
        return var->value;
}

nodiscard VAR_VALUE_T * nullable var_set(VAR_T * nonnull var,
                                         VAR_VALUE_T * nullable value,
                                         BITMASK32_T flags) {
        VAR_VALUE_T * new_value;
        VAR_VALUE_T * old;

        if (!s_own_flags_valid(flags))
                return VAR_VALUE_ERR;
        if (s_var_is_readonly(var))
                return VAR_VALUE_ERR;
        if (var->call && var->call_ops != NULL && var->call_ops->set != NULL)
                return s_own_callback_result(var->call_ops->set(var, value, flags), flags);
        new_value = s_own_new_value(value, flags);
        if (var_value_is_err(new_value))
                return VAR_VALUE_ERR;
        old = var->value;
        if (old == new_value)
                return NULL;
        var->value = new_value;
        return s_own_old_result(old, flags);
}

nodiscard VAR_VALUE_T * nullable var_delete(VAR_T * nonnull var, BITMASK32_T flags) {
        VAR_VALUE_T * old;

        if (!s_own_flags_valid(flags))
                return VAR_VALUE_ERR;
        if (s_var_is_readonly(var))
                return VAR_VALUE_ERR;
        if (var->call && var->call_ops != NULL && var->call_ops->unset != NULL)
                return s_own_callback_result(var->call_ops->unset(var, flags), flags);
        old        = var->value;
        var->value = NULL;
        return s_own_old_result(old, flags);
}

SUCCESS_T var_read(VAR_T * nonnull var, VAR_VALUE_T * nonnull out) {
        VAR_VALUE_T * value = var_get(var);

        if (var_value_is_err(value))
                return FAIL;
        if (value == NULL) {
                var_value_init_unset(out);
                return SUCCESS;
        }
        return var_value_copy(out, value);
}

SUCCESS_T var_write(VAR_T * nonnull var, const VAR_VALUE_T * nonnull value) {
        VAR_VALUE_T * copy = var_value_clone(value);
        VAR_VALUE_T * old;

        if (var_value_is_err(copy))
                return FAIL;
        old = var_set(var, copy, VAR_OWN_REPLACE);
        if (var_value_is_err(old)) {
                var_value_free(copy);
                return FAIL;
        }
        return SUCCESS;
}

SUCCESS_T var_chattr(VAR_T * nonnull var, BITMASK32_T set_attr, BITMASK32_T clear_attr) {
        BITMASK32_T next;

        if (s_var_is_readonly(var) && clear_attr != 0)
                return FAIL;
        next = (var->attr | set_attr) & ~clear_attr;
        if ((next & (BITMASK32_T)VAR_ATTR_INDEXED) != 0
            && (next & (BITMASK32_T)VAR_ATTR_ASSOC) != 0)
                return FAIL;
        if ((next & (BITMASK32_T)VAR_ATTR_NAMEREF) != 0
            && (next & ((BITMASK32_T)VAR_ATTR_INDEXED | (BITMASK32_T)VAR_ATTR_ASSOC)) != 0)
                return FAIL;
        var->attr = next;
        return SUCCESS;
}

void var_set_call(VAR_T * nonnull var, const VAR_CALL_OPS_T * nullable ops, void * nullable data) {
        var->call      = ops != NULL;
        var->call_ops  = ops;
        var->call_data = data;
}

nodiscard VAR_VALUE_T * nullable var_array_get(VAR_T * nonnull var,
                                               const VAR_SUBSCRIPT_T * nonnull subscript) {
        VAR_VALUE_T * stored = var_get(var);
        VAR_ARRAY_T * array;

        if (var_value_is_err(stored))
                return VAR_VALUE_ERR;
        if (stored == NULL)
                return NULL;
        array = s_value_array(stored);
        if (array == NULL)
                return VAR_VALUE_ERR;
        if (array->call && array->call_ops != NULL && array->call_ops->get_elem != NULL)
                return array->call_ops->get_elem(var, subscript);
        return hash_get(array->values, subscript->text, subscript->len);
}

nodiscard VAR_VALUE_T * nullable var_array_set(VAR_T * nonnull var,
                                               const VAR_SUBSCRIPT_T * nonnull subscript,
                                               VAR_VALUE_T * nullable value,
                                               BITMASK32_T flags) {
        VAR_ARRAY_T * array;

        if (!s_own_flags_valid(flags))
                return VAR_VALUE_ERR;
        if (s_var_is_readonly(var))
                return VAR_VALUE_ERR;
        array = s_var_array_for_write(var);
        if (var_ptr_is_err(array))
                return VAR_VALUE_ERR;
        if (array->call && array->call_ops != NULL && array->call_ops->set_elem != NULL)
                return s_own_callback_result(
                    array->call_ops->set_elem(var, subscript, value, flags), flags);
        return s_array_set_value(array, subscript, value, flags);
}

nodiscard VAR_VALUE_T * nullable var_array_delete(VAR_T * nonnull var,
                                                  const VAR_SUBSCRIPT_T * nonnull subscript,
                                                  BITMASK32_T flags) {
        VAR_VALUE_T * stored;
        VAR_ARRAY_T * array;

        if (!s_own_flags_valid(flags))
                return VAR_VALUE_ERR;
        if (s_var_is_readonly(var))
                return VAR_VALUE_ERR;
        stored = var_get(var);
        if (var_value_is_err(stored))
                return VAR_VALUE_ERR;
        if (stored == NULL)
                return NULL;
        array = s_value_array(stored);
        if (array == NULL)
                return VAR_VALUE_ERR;
        if (array->call && array->call_ops != NULL && array->call_ops->unset_elem != NULL)
                return s_own_callback_result(array->call_ops->unset_elem(var, subscript, flags),
                                             flags);
        return s_array_delete_value(array, subscript, flags);
}

nodiscard VAR_VALUE_T * nullable var_array_keys(VAR_T * nonnull var) {
        VAR_VALUE_T * stored = var_get(var);
        VAR_VALUE_T * keys;
        VAR_ARRAY_T * array;
        size_t index = 0;

        if (var_value_is_err(stored))
                return VAR_VALUE_ERR;
        if (stored == NULL)
                return s_value_create_array(VAR_ARRAY_INDEXED);
        array = s_value_array(stored);
        if (array == NULL)
                return VAR_VALUE_ERR;
        if (array->call && array->call_ops != NULL && array->call_ops->keys != NULL)
                return array->call_ops->keys(var);
        keys = s_value_create_array(VAR_ARRAY_INDEXED);
        for (HASH_ENTRY_T * entry = array->values->head; entry != NULL; entry = hash_next(entry)) {
                char key_index[sizeof(size_t) * CHAR_BIT / 3 + 3];
                int written = snprintf(key_index, sizeof(key_index), "%zu", index);
                VAR_VALUE_T * key_value;

                if (written < 0 || (size_t)written >= sizeof(key_index))
                        s_var_abort("array key index overflow");
                key_value = xmalloc(sizeof(*key_value));
                if (var_value_init_string_len(key_value, (const char *)entry->key, entry->key_len)
                    != SUCCESS)
                        s_var_abort("array key clone failed");
                hash_set(keys->as.array->values, key_index, (size_t)written, key_value);
                index++;
        }
        return keys;
}

SUCCESS_T var_array_length(VAR_T * nonnull var, size_t * nonnull out) {
        VAR_VALUE_T * stored = var_get(var);
        VAR_ARRAY_T * array;

        if (var_value_is_err(stored))
                return FAIL;
        if (stored == NULL) {
                *out = 0;
                return SUCCESS;
        }
        array = s_value_array(stored);
        if (array == NULL)
                return FAIL;
        if (array->call && array->call_ops != NULL && array->call_ops->length != NULL)
                return array->call_ops->length(var, out);
        *out = array->values->size;
        return SUCCESS;
}
