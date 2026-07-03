/* vim: set ts=8 sw=8 sts=8 et: */
#include "include/evilgeneral.h"

#include "bind.h"
#include "libs/memm.h"
#include <locale.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>
#include <wchar.h>

struct termios orig_termios;
BIND_NODE_T * root = NULL;

/* 这些应该被装进头文件里。*/

typedef struct {
        uint8_t size;  // 1,2,3,4
        uint8_t width; // 0,1,2
} CHAR_META_T;

typedef struct line_unit_str {
        struct line_unit_str * prev;
        struct line_unit_str * next;
        uint32_t lineno;    // 行号，从1开始
        uint32_t size;      // 当前行中有效字符数
        uint32_t width;     // 当前行显示宽度；此处简单认为等于 size（仅ASCII）
        uint32_t capacity;  // 缓冲区总容量（字节数）
        CHAR_META_T * meta; // 每个字符的元信息
        char * buffer;      // 文本缓冲区，以'\0'结尾，长度不超过 capacity
} LINE_UNIT_T;

typedef struct le_context_str {
        char * ps0;
        char * ps1;
        char * ps2;
        char * ps3;
        char * ps4;
        char * ctxbuffer; // 默认16字节。
        uint32_t lineno;  // 当前行号，从1开始
        uint32_t point;   // 这是每行结构体中buffer的索引。用于快速定位字节。
        uint16_t column;  // 这是期望视觉列变量。
        // 这个东西应当随着遍历charmeta计算。如果太大，大于终端尺寸是需要横滚或折行的

        uint16_t cursorx; // 光标横偏移
        uint16_t cursory; // 光标纵偏移

        [[maybe_unused]] uint8_t flags; // 见上面define.目前unused.
        uint32_t metachar;              // 这是每行结构体中 meta的索引，即当前所在字符。
        LINE_UNIT_T * root_line;
        LINE_UNIT_T * cur_line;
        uint32_t done; // 非0表示结束编辑循环
} LE_CONTEXT_T;

LE_CONTEXT_T le_ctx;
LINE_UNIT_T * root_line = NULL;

// end

SUCCESS_T cursor_left(LE_CONTEXT_T * nonnull lectx, uint16_t steps) {
        if (steps && lectx->cursorx >= steps) {
                printf("\033[%dD", steps);
                lectx->cursorx -= steps;
                return 0;
        }
        return 1;
}

SUCCESS_T cursor_right(LE_CONTEXT_T * nonnull lectx, uint16_t steps) {
        if (steps) {
                printf("\033[%dC", steps);
                lectx->cursorx += steps;
                return 0;
        }
        return 1;
}

SUCCESS_T cursor_up(LE_CONTEXT_T * nonnull lectx, uint16_t steps) {
        if (steps && lectx->cursory >= steps) {
                printf("\033[%dA", steps);
                lectx->cursory -= steps;
                return 0;
        }
        return 1;
}

SUCCESS_T cursor_down(LE_CONTEXT_T * nonnull lectx, uint16_t steps) {
        if (steps) {
                printf("\033[%dB", steps);
                lectx->cursory += steps;
                return 0;
        }
        return 1;
}

void * nonnull nmalloc(size_t elements, size_t element_size) {
        void * ptr = ecalloc(elements, element_size);
        if (!ptr) {
                perror("nmalloc");
                exit(1);
        }
        return ptr;
}

uint16_t strwidth(char * nonnull string) { return; }

// 完成!!久久!!完成!!<@lvq_14907>
/* 提交当前多行缓冲区
 * 当前实现：输出所有行内容到标准输出，然后结束编辑循环。
 */
SUCCESS_T accept_line(LE_CONTEXT_T * lectx);

/* 添加字符 */
SUCCESS_T insert_char(LE_CONTEXT_T * lectx);

/* 删除字符：删除光标前一个字符 */
SUCCESS_T del_char(LE_CONTEXT_T * lectx);

/* 左移光标：在行首时移动到上一行末尾 */
SUCCESS_T le_left(LE_CONTEXT_T * lectx);

/* 右移光标：在行尾时移动到下一行行首 */
SUCCESS_T le_right(LE_CONTEXT_T * lectx);

/* 上移行：尽量保持同一列 */
SUCCESS_T le_up(LE_CONTEXT_T * lectx);

/* 下移行：尽量保持同一列 */
SUCCESS_T le_down(LE_CONTEXT_T * lectx);

/* 创建新行：在当前光标处拆分当前行 */
SUCCESS_T new_line(LE_CONTEXT_T * lectx);

/* 结束编辑 */
SUCCESS_T finish(LE_CONTEXT_T * lectx);

/* 重绘全部行：清屏后打印所有行并移动光标到当前行列 */
SUCCESS_T redraw_all(LE_CONTEXT_T * lectx);

/* 重绘单行：当前实现直接调用重绘全部 */
SUCCESS_T redraw_line(LE_CONTEXT_T * lectx);

SUCCESS_T input_loop(LE_CONTEXT_T * nonnull lectx) {
        uint8_t confused = 0;
        char c           = 0;

        while (!lectx->done) {
                void * nullable addr = NULL;

                /* 清空本次序列的上下文缓冲区 */
                if (lectx->ctxbuffer) {
                        lectx->ctxbuffer[0] = '\0';
                }

                while (!lectx->done) {
                        if (confused == 0) {
                                /* 读取一个字节；简单处理错误情况 */
                                int n = read(STDIN_FILENO, &c, 1);
                                if (n <= 0) {
                                        puts("该死，read读取错误");
                                        continue;
                                }
                        } else {
                                /* confused==1: 本次不再读取新字符，只是复用上一次的 c */
                                confused = 0;
                        }

                        SUCCESS_T ret = further_match(root, &addr, c);
                        if (ret == 1) {
                                /* 仍在匹配一个按键序列：把当前字符压入 ctxbuffer，等待下一个字符 */
                                if (lectx->ctxbuffer) {
                                        size_t len                = strlen(lectx->ctxbuffer);
                                        lectx->ctxbuffer[len]     = c;
                                        lectx->ctxbuffer[len + 1] = '\0';
                                }
                                continue;
                        }

                        if (ret == 0) {
                                /* 匹配结束：调用绑定函数（此处仅占位），然后清空缓冲区 */
                                if (addr) {
                                        /* TODO: 调用绑定的行编辑函数，例如将 lectx 作为参数传入。
                                         */
                                }
                                if (lectx->ctxbuffer) {
                                        lectx->ctxbuffer[0] = '\0';
                                }
                                /* 本次按键序列处理完毕，返回外层循环等待下一次输入 */
                                break;
                        }

                        if (ret == -1) {
                                /* 匹配失败：调用默认处理（占位），清空缓冲区并标记 confused */
                                if (addr) {
                                        /* TODO: 调用默认处理函数，使用当前的 ctxbuffer 和字符 c。
                                         */
                                }
                                if (lectx->ctxbuffer) {
                                        lectx->ctxbuffer[0] = '\0';
                                }
                                confused = 1;
                                /* 跳出内层循环，使得下一轮在不读取新字符的情况下再次尝试匹配 c */
                                break;
                        }
                }
        }

        return 0;
}

int main(int argc, char * argv[]) {
        root         = initialize_keymap();
        long timeout = 0;

        tcgetattr(STDIN_FILENO, &orig_termios);
        struct termios raw = orig_termios;
        /* 关闭规范模式和回显，并关闭 CR/LF 自动转换，以便在程序中区分 '\r' 和 '\n' */
        raw.c_lflag &= ~(ICANON | ECHO);
        raw.c_iflag &= ~(ICRNL | INLCR);
        raw.c_oflag &= ~(OPOST | ONLCR);
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        setvbuf(stdout, NULL, _IONBF, 0);

        /* 绑定按键：
         * Ctrl+J (LF, 0x0a) 提交多行并结束；
         * Ctrl+M/回车 (CR, 0x0d) 创建新行；
         * Ctrl+Q (0x11) 结束编辑；
         * Backspace (0x7f) 删除字符。
         */
        add_bind(root, timeout, "\x0a", accept_line);
        add_bind(root, timeout, "\x0d", new_line);
        add_bind(root, timeout, "\x11", finish);
        add_bind(root, timeout, "\x7f", del_char);

        /* 初始化根行和编辑上下文 */

        input_loop(&le_ctx);

        /* 恢复终端设置并释放资源 */
        tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);

        return 0;
}
