#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

int main(int argc, char * argv[]) {
        uint_fast8_t buf;
        struct termios origin_termios, mod_termios;
        tcgetattr(STDIN_FILENO, &origin_termios);
        mod_termios = origin_termios;
        mod_termios.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &mod_termios);
        tcsetattr(STDOUT_FILENO, TCSANOW, &mod_termios);
        while (1) {
                read(STDIN_FILENO, &buf, 1);

                if (buf == '\n') {
                        printf("\n");
                } else {
                        printf("%X", buf);
                        fflush(stdout);
                }
        }
        return EXIT_SUCCESS;
}
