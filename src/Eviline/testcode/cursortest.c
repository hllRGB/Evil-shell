#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

int main(void) {

        struct termios origin_termios;
        tcgetattr(STDIN_FILENO, &origin_termios);

        struct termios raw_termios = origin_termios;

        raw_termios.c_lflag &= ~(ICANON | ECHO);

        tcsetattr(STDIN_FILENO, TCSANOW, &raw_termios);
        system("clear");
        uint8_t key = 0;
        while (read(STDIN_FILENO, &key, 1) == 1) {
                switch (key) {
                case 'h':
                        printf("\033[1D"); // 左
                        break;
                case 'j':
                        printf("\033[1B"); // 下
                        break;
                case 'k':
                        printf("\033[1A"); // 上
                        break;
                case 'l':
                        printf("\033[1C"); // 右
                        break;
                case 'q':
                        goto end;
                        break;
                default:
                        printf("%c", key); // 键
                        break;
                }
                fflush(stdout);
        }
end:
        tcsetattr(STDIN_FILENO, TCSANOW, &origin_termios);
        return 0;
}
