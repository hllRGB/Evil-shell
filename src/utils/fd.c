#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
uint_fast8_t state = 0;
char byte;
uint16_t lists[112] = {
    0xCC80, 0xCC81, 0xCC82, 0xCC83, 0xCC84, 0xCC85, 0xCC86, 0xCC87, 0xCC88, 0xCC89, 0xCC8A, 0xCC8B,
    0xCC8C, 0xCC8D, 0xCC8E, 0xCC8F, 0xCC90, 0xCC91, 0xCC92, 0xCC93, 0xCC94, 0xCC95, 0xCC96, 0xCC97,
    0xCC98, 0xCC99, 0xCC9A, 0xCC9B, 0xCC9C, 0xCC9D, 0xCC9E, 0xCC9F, 0xCCA0, 0xCCA1, 0xCCA2, 0xCCA3,
    0xCCA4, 0xCCA5, 0xCCA6, 0xCCA7, 0xCCA8, 0xCCA9, 0xCCAA, 0xCCAB, 0xCCAC, 0xCCAD, 0xCCAE, 0xCCAF,
    0xCCB0, 0xCCB1, 0xCCB2, 0xCCB3, 0xCCB4, 0xCCB5, 0xCCB6, 0xCCB7, 0xCCB8, 0xCCB9, 0xCCBA, 0xCCBB,
    0xCCBC, 0xCCBD, 0xCCBE, 0xCCBF, 0xCD80, 0xCD81, 0xCD82, 0xCD83, 0xCD84, 0xCD85, 0xCD86, 0xCD87,
    0xCD88, 0xCD89, 0xCD8A, 0xCD8B, 0xCD8C, 0xCD8D, 0xCD8E, 0xCD8F, 0xCD90, 0xCD91, 0xCD92, 0xCD93,
    0xCD94, 0xCD95, 0xCD96, 0xCD97, 0xCD98, 0xCD99, 0xCD9A, 0xCD9B, 0xCD9C, 0xCD9D, 0xCD9E, 0xCD9F,
    0xCDA0, 0xCDA1, 0xCDA2, 0xCDA3, 0xCDA4, 0xCDA5, 0xCDA6, 0xCDA7, 0xCDA8, 0xCDA9, 0xCDAA, 0xCDAB,
    0xCDAC, 0xCDAD, 0xCDAE, 0xCDAF};

int parse_utf8(void) {
        switch (state) {
        case 0:
                if ((byte & 0x80) == 0x00) {
                        // ASCII
                        state = 1;
                } else if ((byte & 0xE0) == 0xC0) {
                        // 2 字节
                        state = 2;
                } else if ((byte & 0xF0) == 0xE0) {
                        // 3 字节
                        state = 3;
                } else if ((byte & 0xF8) == 0xF0) {
                        // 4 字节
                        state = 4;
                } else {
                        // 非法
                        state = 0;
                        return -1;
                }
        default:
                return 0;
        }
}
// 最大255
#define COUNT 40
void print_fdwb(void) {
        uint_fast8_t count = rand() % (COUNT + 1);
        for (uint_fast8_t i = 0; i <= count; ++i) {
                uint_fast16_t selected   = lists[rand() % 112]; // 获取随机发电字符
                uint_fast8_t reversed[2] = {                    // 给字节序翻一下
                                            (selected >> 8) & 0xFF,
                                            selected & 0xFF};
                write(STDOUT_FILENO, &reversed, 2); // 写
        }
}

int main(int argc, char * argv[]) {
        // 调终端
        struct termios origin_termios_attribute;
        tcgetattr(STDIN_FILENO, &origin_termios_attribute);
        struct termios init_termios_attribute = origin_termios_attribute;
        init_termios_attribute.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &init_termios_attribute);
        setvbuf(stdout, NULL, _IONBF, 0);

        srand((unsigned int)time(NULL));

        while (1) { // 开始操作
                read(STDIN_FILENO, &byte, 1);
                if (byte == '\n') {
                        printf("\n");
                } else {
                        if (state) {
                                write(STDOUT_FILENO, &byte, 1);
                                if (--state == 0) {
                                        print_fdwb();
                                }
                        } else {
                                parse_utf8();
                                write(STDOUT_FILENO, &byte, 1);
                                if (--state == 0) {
                                        print_fdwb();
                                }
                        }
                }
        }
        return EXIT_SUCCESS;
}
