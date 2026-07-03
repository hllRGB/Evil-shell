#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

int utf8_classify(uint8_t byte) {
        if ((byte & 0b10000000) == 0b00000000)
                return 1;
        if ((byte & 0b11000000) == 0b10000000)
                return 0;
        if ((byte & 0b11100000) == 0b11000000) {
                if (byte == 0b11000000 || byte == 0b11000001)
                        return -1;
                return 2;
        }
        if ((byte & 0b11110000) == 0b11100000)
                return 3;
        if ((byte & 0b11111000) == 0b11110000) {
                if (byte > 0b11110010)
                        return -1;
                return 4;
        }
        return -1;
}

int main(void) {
        const char * test = "测试👿";
        for (size_t i = 0; test[i]; ++i) {
                printf("byte[%2zu] 0x%02X\n", i, test[i]);
                int ret = utf8_classify(test[i]);
                switch (ret) {
                case 1:
                case 2:
                case 3:
                case 4:
                        printf("HEAD with length %d\n", ret);
                        break;
                case 0:
                        printf("BODY\n");
                        break;
                case -1:
                        printf("INVALID\n");
                        break;
                default:
                        printf("UNDEFINED\n");
                        break;
                }
        }
        return 0;
}
