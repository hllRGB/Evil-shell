// SPDX-License-Identifier: GPL-3.0-or-later
// unicode_recognize.c — inspect UTF-8 byte sequences

#include <stdint.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

enum { H1, H2, H3, H4, CONT, INV };

static int byte_kind(uint8_t b) {
        if (b <= 0x7F)
                return H1;
        if (b <= 0xBF)
                return CONT;
        if (b <= 0xC1)
                return INV; // 0xC0-0xC1: overlong 2-byte header
        if (b <= 0xDF)
                return H2;
        if (b <= 0xEF)
                return H3;
        if (b <= 0xF4)
                return H4;
        return INV; // 0xF5-0xFF: invalid
}

static int32_t decode_seq(const uint8_t * s, int n) {
        int32_t cp;
        switch (n) {
        case 2:
                cp = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
                return (cp >= 0x80) ? cp : -1;
        case 3:
                cp = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
                if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF))
                        return -1;
                return cp;
        case 4:
                cp = ((s[0] & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6)
                     | (s[3] & 0x3F);
                return (cp >= 0x10000 && cp <= 0x10FFFF) ? cp : -1;
        default:
                return -1;
        }
}

static void emit_cp(int32_t cp) {
        if (cp <= 0x7F)
                putchar(cp);
        else if (cp <= 0x7FF) {
                putchar(0xC0 | (cp >> 6));
                putchar(0x80 | (cp & 0x3F));
        } else if (cp <= 0xFFFF) {
                putchar(0xE0 | (cp >> 12));
                putchar(0x80 | ((cp >> 6) & 0x3F));
                putchar(0x80 | (cp & 0x3F));
        } else {
                putchar(0xF0 | (cp >> 18));
                putchar(0x80 | ((cp >> 12) & 0x3F));
                putchar(0x80 | ((cp >> 6) & 0x3F));
                putchar(0x80 | (cp & 0x3F));
        }
}

int main(int argc, char ** argv) {
        setvbuf(stdout, NULL, _IONBF, 0);
        FILE * fp = stdin;
        setvbuf(fp, NULL, _IONBF, 0);
        if (argc > 1) {
                fp = fopen(argv[1], "rb");
                if (!fp) {
                        perror(argv[1]);
                        return 1;
                }
                setvbuf(fp, NULL, _IONBF, 0);
        }

        struct termios saved, *tty = NULL;
        if (fp == stdin && isatty(STDIN_FILENO)) {
                tcgetattr(STDIN_FILENO, &saved);
                struct termios raw = saved;
                raw.c_lflag &= ~(ICANON | ECHO);
                tcsetattr(STDIN_FILENO, TCSANOW, &raw);
                tty = &saved;
        }

        printf("fmt:off byte kind [U+XXXX CHAR] [err]\n");

        uint8_t seq[4];
        int seqlen = 0, need = 0;
        int off = 0;

        for (int c; (c = fgetc(fp)) != EOF; off++) {
                uint8_t b = (uint8_t)c;
                printf("%d %02x", off, b);

                int k = byte_kind(b);

                // flush incomplete sequence on new header or invalid
                if (seqlen && (k == H1 || k == INV || (k >= H2 && k <= H4))) {
                        printf(" drop_seq(%d)", seqlen);
                        seqlen = need = 0;
                }

                switch (k) {
                case H1:
                        printf(" H1 U+%04X ", b);
                        putchar(b);
                        break;
                case CONT:
                        if (seqlen && need) {
                                seq[seqlen++] = b;
                                if (--need == 0) {
                                        int32_t cp = decode_seq(seq, seqlen);
                                        if (cp < 0)
                                                printf(" CONT INV:bad_seq");
                                        else {
                                                printf(" CONT U+%04X ", cp);
                                                emit_cp(cp);
                                        }
                                        seqlen = 0;
                                } else
                                        printf(" CONT");
                        } else
                                printf(" CONT INV:orphan");
                        break;
                case H2:
                        seq[0] = b;
                        seqlen = 1;
                        need   = 1;
                        printf(" H2");
                        break;
                case H3:
                        seq[0] = b;
                        seqlen = 1;
                        need   = 2;
                        printf(" H3");
                        break;
                case H4:
                        seq[0] = b;
                        seqlen = 1;
                        need   = 3;
                        printf(" H4");
                        break;
                default:
                        printf(" INV:bad_byte");
                        break;
                }
                printf("\n");
        }

        if (seqlen)
                printf("-- eof partial_seq %d\n", seqlen);

        if (tty)
                tcsetattr(STDIN_FILENO, TCSANOW, tty);
        if (fp != stdin)
                fclose(fp);
        return 0;
}
