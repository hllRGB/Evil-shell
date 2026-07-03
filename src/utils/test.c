#include <stdio.h>
#include <string.h>
#include <time.h>

#define TEST_TIMES 10000000

// 正确关键字：return
// 待匹配候选："returna"、"returm"、"return"

// ==============================
// 版本1：长度预选 + 与逻辑（你要的极致版）
// ==============================
static inline int is_return_fast(const char *s)
{
    // 长度先过滤：return 是 6 字节 + \0 → 总长7
    if (s[6] != 0)
        return 0;

    return (s[0] == 'r'
         && s[1] == 'e'
         && s[2] == 't'
         && s[3] == 'u'
         && s[4] == 'r'
         && s[5] == 'n'
         && s[6] == 0);
}

// ==============================
// 版本2：strcmp 暴力匹配
// ==============================
static inline int is_return_strcmp(const char *s)
{
    if (strcmp(s, "returna") == 0)
        return 0;
    if (strcmp(s, "returm") == 0)
        return 0;
    if (strcmp(s, "return") == 0)
        return 1;
    return 0;
}

// ==============================
// 通用测速函数
// ==============================
void bench(const char *name, int (*is_target)(const char*))
{
    const char *test_strings[] = {
        "returna",
        "returm",
        "return"
    };
    int n = sizeof(test_strings) / sizeof(test_strings[0]);

    clock_t start = clock();

    volatile int count = 0;
    for (long i = 0; i < TEST_TIMES; i++) {
        const char *s = test_strings[i % n];
        count += is_target(s);
    }

    clock_t end = clock();
    double ms = (double)(end - start) * 1000 / CLOCKS_PER_SEC;

    printf("%-20s: %.2f ms | 正确匹配次数 = %d\n", name, ms, count);
}

int main()
{
    printf("测试次数：%d 万次\n\n", TEST_TIMES / 10000);

    bench("长度+与逻辑(快版)", is_return_fast);
    bench("strcmp暴力版     ", is_return_strcmp);

    return 0;
}
