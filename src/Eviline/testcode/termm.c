#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char * argv[]) {
        for (int i = 0; i <= 20; i++) {
                write(STDOUT_FILENO, "fuck", 5);
                sleep(1);
        }
        return EXIT_SUCCESS;
}
