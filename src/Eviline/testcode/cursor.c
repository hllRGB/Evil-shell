#include <stdio.h>
void cursor_left(void) { printf("\033[D"); }
void cursor_right(void) { printf("\033[C"); }
void cursor_up(void) { printf("\033[A"); }
void cursor_down(void) { printf("\033[B"); }
