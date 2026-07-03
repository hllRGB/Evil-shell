#include <stdint.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

typedef enum { STATE_IDLE, STATE_WAIT_2ND, STATE_WAIT_3RD, STATE_WAIT_4TH } Utf8State;

int utf8_parse(uint8_t buf, Utf8State * state, uint32_t * unicode) {
        static uint32_t temp_code;

        switch (*state) {
        case STATE_IDLE:
                if ((buf & 0x80) == 0x00) {
                        // ASCII
                        *unicode = buf;
                        return 1;
                } else if ((buf & 0xE0) == 0xC0) {
                        // 2 字节
                        temp_code = (buf & 0x1F) << 6;
                        *state    = STATE_WAIT_2ND;
                        return 0;
                } else if ((buf & 0xF0) == 0xE0) {
                        // 3 字节
                        temp_code = (buf & 0x0F) << 12;
                        *state    = STATE_WAIT_3RD;
                        return 0;
                } else if ((buf & 0xF8) == 0xF0) {
                        // 4 字节
                        temp_code = (buf & 0x07) << 18;
                        *state    = STATE_WAIT_4TH;
                        return 0;
                } else {
                        // 非法）
                        *state = STATE_IDLE;
                        return -1;
                }

        case STATE_WAIT_2ND:
                if ((buf & 0xC0) != 0x80)
                        return -1;
                *unicode = temp_code | (buf & 0x3F);
                *state   = STATE_IDLE;
                return 2;

        case STATE_WAIT_3RD:
                if ((buf & 0xC0) != 0x80)
                        return -1;
                temp_code |= (buf & 0x3F) << 6;
                *state = STATE_WAIT_2ND;
                return 0;

        case STATE_WAIT_4TH:
                if ((buf & 0xC0) != 0x80)
                        return -1;
                temp_code |= (buf & 0x3F) << 12;
                *state = STATE_WAIT_3RD;
                return 0;

        default:
                *state = STATE_IDLE;
                return -1;
        }
}

int main() {
        uint8_t buf;
        Utf8State state = STATE_IDLE;
        uint32_t unicode;
        int ret;
        struct termios origin_termios_attribute;
        tcgetattr(STDIN_FILENO, &origin_termios_attribute);
        struct termios init_termios_attribute = origin_termios_attribute;

        init_termios_attribute.c_lflag &= ~(ICANON | ECHO);

        tcsetattr(STDIN_FILENO, TCSANOW, &init_termios_attribute);

        // 循环读取 TTY 输入（每次读 1 字节，模拟单字节缓冲区）
        while (read(STDIN_FILENO, &buf, 1) == 1) {
                ret = utf8_parse(buf, &state, &unicode);
                switch (ret) {
                case 1:
                        printf("ASCII ：%c（Unicode: U+%04X）\n", (char)unicode, unicode);
                        break;
                case 2:
                        printf("Unicode (UTF-8）：U+%04X\n", unicode);
                        break;
                case -1:
                        printf("非法（字节值：0x%02X）\n", buf);
                        break;
                default:
                        break;
                }
        }

        return 0;
}
