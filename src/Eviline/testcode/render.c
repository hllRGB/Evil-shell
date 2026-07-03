// SPDX-License-Identifier: GPL-3.0-or-later
// render.c — loading animation for 3x46 prompt frame

#define _GNU_SOURCE 1
#include "cursor.h"
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <wchar.h>

#define WIDTH 46
#define HEIGHT 3
#define MAX_CHARS 128

static const char * lines[HEIGHT] = {
    "┌─[-bash-245720 21:43:04 hllrgb@Evil ~]-[arch]",
    "└─$>>_                                        ",
    "-- INSERT --                                  ",
};

static struct {
        int offset;
        int blen;
        int cols;
} cinfo[HEIGHT][MAX_CHARS];

static int nchars[HEIGHT];
static int colmap[HEIGHT][WIDTH];

static uint32_t decode(const char * s, int * blen) {
        uint8_t b = (uint8_t)*s;
        if (b <= 0x7F) {
                *blen = 1;
                return b;
        }
        if (b <= 0xBF) {
                *blen = 1;
                return b;
        }
        int n;
        uint32_t cp;
        if (b <= 0xDF) {
                cp = b & 0x1F;
                n  = 2;
        } else if (b <= 0xEF) {
                cp = b & 0x0F;
                n  = 3;
        } else if (b <= 0xF4) {
                cp = b & 0x07;
                n  = 4;
        } else {
                *blen = 1;
                return b;
        }
        for (int i = 1; i < n; i++) {
                if ((s[i] & 0xC0) != 0x80) {
                        *blen = i;
                        return (uint8_t)s[0];
                }
                cp = (cp << 6) | (s[i] & 0x3F);
        }
        *blen = n;
        return cp;
}

static int cp_cols(uint32_t cp) {
        int w = wcwidth((wchar_t)cp);
        return (w >= 1) ? w : 1;
}

static void init_text(void) {
        for (int r = 0; r < HEIGHT; r++) {
                for (int c = 0; c < WIDTH; c++)
                        colmap[r][c] = -1;

                const char * s = lines[r];
                int off = 0, ci = 0, col = 0;
                while (*s && ci < MAX_CHARS && col < WIDTH) {
                        int blen;
                        uint32_t cp = decode(s, &blen);
                        int cw      = cp_cols(cp);
                        if (col + cw > WIDTH)
                                break;

                        cinfo[r][ci].offset = off;
                        cinfo[r][ci].blen   = blen;
                        cinfo[r][ci].cols   = cw;
                        for (int j = 0; j < cw; j++)
                                colmap[r][col + j] = ci;
                        ci++;
                        col += cw;
                        s += blen;
                        off += blen;
                }
                nchars[r] = ci;
        }
}

// render frame into buf. revealed[r][c]=1 means text visible at (r,c)
static int render(char * buf, int fill[HEIGHT][WIDTH], int revealed[HEIGHT][WIDTH]) {
        int pos = 0;
        for (int r = 0; r < HEIGHT; r++) {
                int col = 0;
                while (col < WIDTH) {
                        int ci = colmap[r][col];
                        if (ci >= 0 && (col == 0 || colmap[r][col - 1] != ci)) {
                                int cw  = cinfo[r][ci].cols;
                                int all = 1;
                                for (int j = 0; j < cw; j++) {
                                        if (!revealed[r][col + j]) {
                                                all = 0;
                                                break;
                                        }
                                }
                                if (all) {
                                        int off  = cinfo[r][ci].offset;
                                        int blen = cinfo[r][ci].blen;
                                        for (int i = 0; i < blen; i++)
                                                buf[pos++] = lines[r][off + i];
                                        col += cw;
                                        continue;
                                }
                        }
                        buf[pos++] = fill[r][col] ? '#' : ' ';
                        col++;
                }
                buf[pos++] = '\n';
        }
        buf[pos] = '\0';
        return pos;
}

int main(void) {
        setlocale(LC_ALL, "");
        init_text();

        int fill[HEIGHT][WIDTH]     = {0};
        int revealed[HEIGHT][WIDTH] = {0};

        FILE * fp = fopen("anim.txt", "w");
        if (!fp) {
                perror("anim.txt");
                return 1;
        }

        char buf[4096];
        int diag_steps = WIDTH + HEIGHT;

        // Phase 1: progressive fill with #
        for (int i = 0; i < diag_steps; i++) {
                int x;
                x = i;
                if (x >= 0 && x < WIDTH)
                        fill[0][x] = 1;
                x = i - 1;
                if (x >= 0 && x < WIDTH)
                        fill[1][x] = 1;
                x = i - 2;
                if (x >= 0 && x < WIDTH)
                        fill[2][x] = 1;

                render(buf, fill, revealed);
                fprintf(fp, "%s", buf);
                if (i < diag_steps - 1)
                        fprintf(fp, "---\n");

                if (i == 0)
                        printf("%s", buf);
                else {
                        for (int j = 0; j < HEIGHT; j++)
                                cursor_up();
                        printf("%s", buf);
                }
                fflush(stdout);
                usleep(1000000 / diag_steps);
        }

        // Phase 2: diagonal reveal of all chars
        for (int i = 0; i < diag_steps; i++) {
                for (int r = 0; r < HEIGHT; r++) {
                        int c = i - r;
                        if (c >= 0 && c < WIDTH) {
                                int ci = colmap[r][c];
                                if (ci >= 0) {
                                        int cw = cinfo[r][ci].cols;
                                        for (int j = 0; j < cw; j++)
                                                revealed[r][c + j] = 1;
                                }
                        }
                }
                render(buf, fill, revealed);
                fprintf(fp, "%s", buf);
                if (i < diag_steps - 1)
                        fprintf(fp, "---\n");

                for (int j = 0; j < HEIGHT; j++)
                        cursor_up();
                printf("%s", buf);
                fflush(stdout);
                usleep(1000000 / diag_steps);
        }

        printf("\n");

        fclose(fp);
        return 0;
}
