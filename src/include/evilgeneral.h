/* vim: set ts=8 sw=8 nu rnu sts=8 et: */
#ifndef SH_EVILGENERAL_H
#define SH_EVILGENERAL_H

#include "detected.h"

// attr tricks
#if defined(__clang__)
#define nonnull _Nonnull
#define nullable _Nullable
#else
#define nonnull
#define nullable
#endif
#define nodiscard [[nodiscard]]
#define FAIL 1
#define SUCCESS 0

typedef uint8_t SUCCESS_T;
typedef uint8_t BITMASK8_T;
typedef uint16_t BITMASK16_T;
typedef uint32_t BITMASK32_T;
typedef uint64_t BITMASK64_T;

#define FLAG_ENABLE(var, mask)                                                 \
        do {                                                                   \
                _Pragma("clang diagnostic push") _Pragma(                      \
                    "clang diagnostic ignored \"-Wenum-conversion\"")(var)     \
                    |= (mask);                                                 \
                _Pragma("clang diagnostic pop")                                \
        } while (0)

#define FLAG_DISABLE(var, mask)                                                \
        do {                                                                   \
                _Pragma("clang diagnostic push") _Pragma(                      \
                    "clang diagnostic ignored \"-Wenum-conversion\"")(var)     \
                    &= ~(mask);                                                \
                _Pragma("clang diagnostic pop")                                \
        } while (0)

// inline trick

#if defined(_MSC_VER)
#define always_inline __forceinline
#elif defined(__GNUC__) // GCC 和 Clang
#define always_inline inline __attribute__((__always_inline__))
#else
#define always_inline
#endif

// string equal
// from bash-5.3

always_inline int STREQ(const char * nonnull a, const char * nonnull b) {
        return (a[0] == b[0] && strcmp(a, b) == 0);
}

// string number equal
// from bash-5.3

always_inline int
STRNEQ(const char * nonnull a, const char * nonnull b, size_t n) {
        return ((n == 0) || (n == 1 && a[0] == b[0])
                || (a[0] == b[0] && strncmp(a, b, n)));
}

#define STRLEN(s)                                                              \
        (((s) && (s)[0]) ? ((s)[1] ? ((s)[2] ? strlen(s) : 2) : 1) : 0)
#define FREE(s)                                                                \
        do {                                                                   \
                if (s)                                                         \
                        free(s);                                               \
        } while (0)

#endif /* SH_EVILGENERAL_H */
