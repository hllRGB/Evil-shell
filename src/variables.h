/* vim: set ts=8 sw=8 nu rnu sts=8 et: */
#ifndef SH_VARIABLES_H
#define SH_VARIABLES_H

#include "include/evilgeneral.h"

#include "hash.h"

typedef intmax_t SH_ARITH_T;
typedef uintmax_t SH_UARITH_T;

typedef struct var_str VAR_T;
typedef struct var_scope_str VAR_SCOPE_T;
typedef struct var_array_str VAR_ARRAY_T;

typedef enum var_value_kind {
        VAR_VALUE_UNSET,
        VAR_VALUE_STRING,
        VAR_VALUE_INT,
        VAR_VALUE_INDEXED_ARRAY,
        VAR_VALUE_ASSOC_ARRAY,
} VAR_VALUE_KIND_T;

typedef enum var_array_kind {
        VAR_ARRAY_INDEXED,
        VAR_ARRAY_ASSOC,
} VAR_ARRAY_KIND_T;

typedef enum var_scope_kind {
        VAR_SCOPE_ENV,
        VAR_SCOPE_GLOBAL,
        VAR_SCOPE_FUNCTION,
        VAR_SCOPE_TEMPORARY,
        VAR_SCOPE_FAST,
} VAR_SCOPE_KIND_T;

typedef enum var_lookup_kind {
        VAR_LOOKUP_NOT_FOUND,
        VAR_LOOKUP_VAR,
        VAR_LOOKUP_TOMBSTONE,
} VAR_LOOKUP_KIND_T;

typedef enum var_attr {
        VAR_ATTR_NONE      = 0,
        VAR_ATTR_EXPORT    = UINT32_C(1) << 0,
        VAR_ATTR_READONLY  = UINT32_C(1) << 1,
        VAR_ATTR_INTEGER   = UINT32_C(1) << 2,
        VAR_ATTR_INDEXED   = UINT32_C(1) << 3,
        VAR_ATTR_ASSOC     = UINT32_C(1) << 4,
        VAR_ATTR_NAMEREF   = UINT32_C(1) << 5,
        VAR_ATTR_LOWERCASE = UINT32_C(1) << 6,
        VAR_ATTR_UPPERCASE = UINT32_C(1) << 7,
        VAR_ATTR_TRACE     = UINT32_C(1) << 8,
        VAR_ATTR_LOCAL     = UINT32_C(1) << 9,
        VAR_ATTR_SPECIAL   = UINT32_C(1) << 10,
        VAR_ATTR_TYPED     = UINT32_C(1) << 11,
        VAR_ATTR_SEALED    = UINT32_C(1) << 12,
        VAR_ATTR_FAST      = UINT32_C(1) << 13,
} VAR_ATTR_T;

typedef enum var_own_flag {
        VAR_OWN_NONE       = 0,
        VAR_OWN_TAKE_NEW   = UINT32_C(1) << 0,
        VAR_OWN_FREE_OLD   = UINT32_C(1) << 1,
        VAR_OWN_RETURN_OLD = UINT32_C(1) << 2,
} VAR_OWN_FLAG_T;

#define VAR_PTR_ERR ((void * nullable)(uintptr_t)UINTPTR_MAX)
#define VAR_VALUE_ERR ((VAR_VALUE_T * nullable)VAR_PTR_ERR)
#define VAR_ERR ((VAR_T * nullable)VAR_PTR_ERR)

#define var_ptr_is_err(ptr) ((const void *)(ptr) == (const void *)VAR_PTR_ERR)
#define var_value_is_err(value) var_ptr_is_err(value)
#define var_is_err(var) var_ptr_is_err(var)

typedef struct var_string_value {
        char * nonnull data;
        size_t len;
} VAR_STRING_VALUE_T;

typedef struct var_value {
        VAR_VALUE_KIND_T kind; // unset, string, int, indexed array, assoc array
        union {
                VAR_STRING_VALUE_T string;
                SH_ARITH_T integer;
                VAR_ARRAY_T * nonnull array;
        } as;
} VAR_VALUE_T;

typedef struct var_subscript {
        const char * nonnull text;
        size_t len;
} VAR_SUBSCRIPT_T;

typedef struct var_lookup {
        VAR_LOOKUP_KIND_T kind; // not found, var, tombstone
        VAR_SCOPE_T * nullable scope;
        VAR_T * nullable var;
} VAR_LOOKUP_T;

typedef struct var_call_ops {
        VAR_VALUE_T * nullable (*nullable get)(VAR_T * nonnull var);
        VAR_VALUE_T * nullable (*nullable set)(VAR_T * nonnull var,
                                              VAR_VALUE_T * nullable value,
                                              BITMASK32_T flags);
        VAR_VALUE_T * nullable (*nullable unset)(VAR_T * nonnull var, BITMASK32_T flags);
} VAR_CALL_OPS_T;

typedef struct var_array_call_ops {
        VAR_VALUE_T * nullable (*nullable get_elem)(VAR_T * nonnull var,
                                                   const VAR_SUBSCRIPT_T * nonnull subscript);
        VAR_VALUE_T * nullable (*nullable set_elem)(VAR_T * nonnull var,
                                                   const VAR_SUBSCRIPT_T * nonnull subscript,
                                                   VAR_VALUE_T * nullable value,
                                                   BITMASK32_T flags);
        VAR_VALUE_T * nullable (*nullable unset_elem)(VAR_T * nonnull var,
                                                     const VAR_SUBSCRIPT_T * nonnull subscript,
                                                     BITMASK32_T flags);
        VAR_VALUE_T * nullable (*nullable keys)(VAR_T * nonnull var);
        SUCCESS_T (*nullable length)(VAR_T * nonnull var, size_t * nonnull out);
        VAR_VALUE_T * nullable (*nullable materialize)(VAR_T * nonnull var);
} VAR_ARRAY_CALL_OPS_T;

typedef enum var_scope_entry_kind {
        VAR_SCOPE_ENTRY_VAR,
        VAR_SCOPE_ENTRY_TOMBSTONE,
} VAR_SCOPE_ENTRY_KIND_T;

typedef struct var_scope_entry {
        VAR_SCOPE_ENTRY_KIND_T kind; // var, tombstone
        VAR_T * nullable var;
} VAR_SCOPE_ENTRY_T;

struct var_array_str {
        VAR_ARRAY_KIND_T kind; // indexed, assoc
        HASHTAB_T * nonnull values;
        bool call;
        const VAR_ARRAY_CALL_OPS_T * nullable call_ops;
        void * nullable call_data;
};

struct var_scope_str {
        HASHTAB_T * nonnull vars;
        VAR_SCOPE_T * nullable next;
        VAR_SCOPE_KIND_T kind; // env, global, function, temporary, fast
};

struct var_str {
        char * nonnull name;
        size_t name_len;
        VAR_VALUE_T * nullable value;
        BITMASK32_T attr; // see VAR_ATTR_T
        bool call;
        const VAR_CALL_OPS_T * nullable call_ops;
        void * nullable call_data;
};

void var_value_init_unset(VAR_VALUE_T * nonnull value);
SUCCESS_T var_value_init_string(VAR_VALUE_T * nonnull value, const char * nonnull string);
SUCCESS_T
var_value_init_string_len(VAR_VALUE_T * nonnull value, const char * nonnull string, size_t len);
void var_value_init_int(VAR_VALUE_T * nonnull value, SH_ARITH_T integer);
SUCCESS_T var_value_copy(VAR_VALUE_T * nonnull dst, const VAR_VALUE_T * nonnull src);
nodiscard VAR_VALUE_T * nullable var_value_clone(const VAR_VALUE_T * nonnull src);
void var_value_destroy(VAR_VALUE_T * nonnull value);
void var_value_free(VAR_VALUE_T * nullable value);

nodiscard VAR_ARRAY_T * nonnull var_array_create(VAR_ARRAY_KIND_T kind);
void var_array_destroy(VAR_ARRAY_T * nullable array);
void var_array_set_call(VAR_ARRAY_T * nonnull array,
                       const VAR_ARRAY_CALL_OPS_T * nullable ops,
                       void * nullable data);

nodiscard bool var_name_is_valid(const char * nonnull name);

nodiscard VAR_SCOPE_T * nonnull var_scope_create(VAR_SCOPE_KIND_T kind, VAR_SCOPE_T * nullable next);
void var_scope_destroy(VAR_SCOPE_T * nullable scope);
nodiscard VAR_LOOKUP_T var_scope_get(VAR_SCOPE_T * nullable start, const char * nonnull name);
nodiscard VAR_LOOKUP_T var_scope_get_in(VAR_SCOPE_T * nonnull scope, const char * nonnull name);
nodiscard VAR_T * nullable var_scope_set(VAR_SCOPE_T * nonnull start,
                                           VAR_SCOPE_T * nonnull fallback,
                                           const char * nonnull name,
                                           VAR_VALUE_T * nullable value,
                                           BITMASK32_T flags,
                                           BITMASK32_T create_attr,
                                           VAR_VALUE_T * nullable * nullable old_out);
nodiscard VAR_T * nullable var_scope_set_in(VAR_SCOPE_T * nonnull scope,
                                              const char * nonnull name,
                                              VAR_VALUE_T * nullable value,
                                              BITMASK32_T flags,
                                              BITMASK32_T create_attr,
                                              VAR_VALUE_T * nullable * nullable old_out);
SUCCESS_T var_scope_delete(VAR_SCOPE_T * nonnull start, const char * nonnull name);
SUCCESS_T var_scope_delete_in(VAR_SCOPE_T * nonnull scope, const char * nonnull name);
SUCCESS_T var_scope_list(VAR_SCOPE_T * nonnull scope,
                        SUCCESS_T (*nonnull callback)(VAR_T * nonnull var,
                                                      void * nullable data),
                        void * nullable data);

nodiscard VAR_LOOKUP_T var_find(VAR_SCOPE_T * nullable start, const char * nonnull name);
nodiscard VAR_LOOKUP_T var_find_in(VAR_SCOPE_T * nonnull scope, const char * nonnull name);
nodiscard VAR_T * nullable var_find_visible(VAR_SCOPE_T * nullable start,
                                                  const char * nonnull name);
nodiscard VAR_T * nullable var_find_in_scope(VAR_SCOPE_T * nonnull scope,
                                                   const char * nonnull name);

SUCCESS_T var_scope_write(VAR_SCOPE_T * nonnull start,
                         VAR_SCOPE_T * nonnull fallback,
                         const char * nonnull name,
                         const VAR_VALUE_T * nonnull value,
                         BITMASK32_T create_attr);
SUCCESS_T var_scope_write_in(VAR_SCOPE_T * nonnull scope,
                            const char * nonnull name,
                            const VAR_VALUE_T * nonnull value,
                            BITMASK32_T create_attr);
SUCCESS_T var_scope_chattr(VAR_SCOPE_T * nonnull start,
                          VAR_SCOPE_T * nonnull fallback,
                          const char * nonnull name,
                          BITMASK32_T set_attr,
                          BITMASK32_T clear_attr,
                          BITMASK32_T create_attr);
SUCCESS_T var_scope_chattr_in(VAR_SCOPE_T * nonnull scope,
                             const char * nonnull name,
                             BITMASK32_T set_attr,
                             BITMASK32_T clear_attr,
                             BITMASK32_T create_attr);
SUCCESS_T var_scope_unset(VAR_SCOPE_T * nonnull start, const char * nonnull name);
SUCCESS_T var_scope_unset_in(VAR_SCOPE_T * nonnull scope, const char * nonnull name);
SUCCESS_T
var_promote(VAR_SCOPE_T * nonnull from, VAR_SCOPE_T * nonnull to, const char * nonnull name);

nodiscard VAR_T * nonnull var_create(const char * nonnull name,
                                           size_t name_len,
                                           BITMASK32_T attr);
void var_destroy(VAR_T * nullable var);
nodiscard VAR_VALUE_T * nullable var_get(VAR_T * nonnull var);
nodiscard VAR_VALUE_T * nullable var_set(VAR_T * nonnull var,
                                           VAR_VALUE_T * nullable value,
                                           BITMASK32_T flags);
nodiscard VAR_VALUE_T * nullable var_delete(VAR_T * nonnull var, BITMASK32_T flags);
nodiscard SUCCESS_T var_read(VAR_T * nonnull var, VAR_VALUE_T * nonnull out);
nodiscard SUCCESS_T var_write(VAR_T * nonnull var, const VAR_VALUE_T * nonnull value);
nodiscard SUCCESS_T var_chattr(VAR_T * nonnull var, BITMASK32_T set_attr, BITMASK32_T clear_attr);
void var_set_call(VAR_T * nonnull var,
                     const VAR_CALL_OPS_T * nullable ops,
                     void * nullable data);

nodiscard VAR_VALUE_T * nullable var_array_get(VAR_T * nonnull var,
                                                 const VAR_SUBSCRIPT_T * nonnull subscript);
nodiscard VAR_VALUE_T * nullable var_array_set(VAR_T * nonnull var,
                                                 const VAR_SUBSCRIPT_T * nonnull subscript,
                                                 VAR_VALUE_T * nullable value,
                                                 BITMASK32_T flags);
nodiscard VAR_VALUE_T * nullable var_array_delete(VAR_T * nonnull var,
                                                    const VAR_SUBSCRIPT_T * nonnull subscript,
                                                    BITMASK32_T flags);
nodiscard VAR_VALUE_T * nullable var_array_keys(VAR_T * nonnull var);
nodiscard SUCCESS_T var_array_length(VAR_T * nonnull var, size_t * nonnull out);

#endif /* SH_VARIABLES_H */
