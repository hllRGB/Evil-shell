/* vim: set ts=8 sw=8 nu rnu sts=8 et: */
#include "include/evilgeneral.h"

#include "bind.h"
#include "libs/memm.h"
// #include <bits/types/struct_timeval.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

fd_set set;
struct timeval timeout;
BIND_NODE_T * nonnull initialize_keymap(void) {
        // The root node owns an array of 256 child nodes.
        BIND_NODE_T * root = emalloc(sizeof(BIND_NODE_T));
        root->type         = NODE;
        root->ptr          = ecalloc(256, sizeof(BIND_NODE_T));
        return root;
}
int rm_bind(BIND_NODE_T * nonnull root, char * nonnull seq) {
        // 约定: 1为未找到该绑定.0为成功解除.-1为未定义行为.
        BIND_NODE_T *first = (BIND_NODE_T *)root->ptr;
        BIND_NODE_T *last  = first;
        // 目前seq不可能为0
        // 遍历字符串.
        for (; *seq; seq++) {
                // 在第一次时位于root所指数组,而它应该不可能被释放.
                // 第一次时将直接访问一次数组。
                // 这里需要注意，取得元素后first应该跟进指针，
                // 因为这个元素马上就会被清零或者free.
                // 后续同逻辑。
                first = ((BIND_NODE_T *)first[(uint8_t)*seq].ptr);
                //                                                        ^在这里跟进指针
                //  这里first已经跟进,last保持.现在需要基于last来清理元素以及数组.
                //  开始进行清理.
                // 现在已经进入字符串节点。判断字符串是否将要结束。
                if (*(seq + 1) != 0) { // 字符串并非快要结束
                        // 那么这里只能是NODE。由于需要完全匹配，如果遇到FUNCTION与NONE，应当报错没有该绑定.
                        if (last[(uint8_t)*seq].type != NODE) {
                                fprintf(stderr, "fuck,没有该绑定!");
                                return 1;
                        }
                        // 异常处理结束.
                        // 开始清理元素.
                        last[(uint8_t)*seq].type = NONE;
                        last[(uint8_t)*seq].ptr  = NULL;
                        // 元素清理完成.
                        // 开始检查是否需要释放数组.
                        if (last != root) { // 检查是不是root所指数组
                                int status = 0;
                                for (uint16_t i = 0; i <= 255; i++) {
                                        status |= last[i].type; // 检查是否数组全空.
                                }
                                if (status == 0) {
                                        efree(last); // 全空则释放它.
                                }
                        }
                        // 数组释放处理完毕.
                        // 清理完毕.
                        last = first;         // 慢指针跟进.
                } else if (*(seq + 1) == 0) { // 字符串将要结束.
                        // 那么这里只能是FUNCTION。由于需要完全匹配，如果遇到NODE与NONE，应当报错没有该绑定.
                        if (last[(uint8_t)*seq].type != FUNCTION) {
                                fprintf(stderr, "fuck,没有该绑定!");
                                return 1;
                        }
                        // 异常处理结束.
                        // 开始进行清理.
                        last[(uint8_t)*seq].type = NONE;
                        last[(uint8_t)*seq].ptr  = NULL;
                        // 元素清理完成.
                        // 开始检查是否需要释放数组.
                        if (last != root) { // 检查是不是root所指数组
                                int status = 0;
                                for (uint16_t i = 0; i <= 255; i++) {
                                        status |= last[i].type; // 检查是否数组全空.
                                }
                                if (status == 0) {
                                        efree(last); // 全空则释放它.
                                }
                        }
                        // 数组释放处理完毕.
                        // 清理完毕.
                        // 现在清理完成，位于末尾，接下来的执行没有必要，直接返回0.
                        return 0;
                }
        }
        return -1; // 遇到未定义行为.
}

int add_bind(BIND_NODE_T * nonnull root, long timeout, char * nonnull seq, void * nonnull func) {
        BIND_NODE_T *node = root;
        // 目前seq不可能为0
        // 目前func不可能为0
        for (; *seq; seq++) {

                node = &(((BIND_NODE_T *)node->ptr)[(uint8_t)*seq]);
                if (*(seq + 1) != 0) {
                        if (node->type == NONE) {
                                node->type = NODE;
                                node->ptr  = ecalloc(256, sizeof(BIND_NODE_T));
                                // Nothing else to do here.
                        } else if (node->type == FUNCTION) {
                                node->type       = NODE;
                                node->timeout_us = timeout;
                                void * tmp       = node->ptr;
                                node->ptr        = ecalloc(256, sizeof(BIND_NODE_T));
                                // Slot 0 is reserved for the fallback action of this prefix node.
                                ((BIND_NODE_T *)node->ptr)[0].type = FUNCTION;
                                ((BIND_NODE_T *)node->ptr)[0].ptr  = tmp;
                                // Nothing else to do here.
                        }
                } else if (*(seq + 1) == 0) {
                        if (node->type == NONE || node->type == FUNCTION) {
                                node->ptr  = func;
                                node->type = FUNCTION;
                                // Return immediately after installing the final binding.
                                return 0;
                        } else if (node->type == NODE) {
                                ((BIND_NODE_T *)node->ptr)[0].type = FUNCTION;
                                ((BIND_NODE_T *)node->ptr)[0].ptr  = func;
                                // Return immediately after installing the final binding.
                                return 0;
                        }
                }
        }
        return 0;
}

int8_t further_match(BIND_NODE_T * nonnull root, void * nullable * nonnull addr, char byte) {
        // 约定:返回值0:成功,-1:失败,1,继续
        static typeof(root) node;
        node = root;
        node = &(((BIND_NODE_T *)node->ptr)[(uint8_t)byte]);
        if (node->type == FUNCTION) {
                // printf("matched\n");
                //((void (*)(void))node->ptr)();
                *addr = node->ptr;
                node  = root;
                return 0;
        } else if (node->type == NONE) {
                // printf("none,");
                *addr = NULL;
                node  = root;
                return -1;
        } else if (node->type == NODE && ((BIND_NODE_T *)node->ptr)[0].type != FUNCTION) {
                // printf("waiting,");
                return 1;
        } else if (node->type == NODE && ((BIND_NODE_T *)node->ptr)[0].type == FUNCTION) {
                // printf("fork,");
                timeout.tv_sec  = 0;
                timeout.tv_usec = (long)node->timeout_us;
                FD_ZERO(&set);
                FD_SET(1, &set);

                if (select(2, &set, NULL, NULL, &timeout) == 0) {
                        // printf("timeout\n");
                        *addr = ((BIND_NODE_T *)node->ptr)[0].ptr;
                        //((void (*)(void))((BIND_NODE_T *)node->ptr)[0].ptr)();
                        node = root;
                        return 0;
                } else {
                        // printf("notimeout,");
                        return 1;
                }
        }
        return -128;
}
