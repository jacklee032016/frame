#include <stdio.h>

int main(void)
{
    union {
        unsigned int x;

        // in little endian system, a is in the endian address, and is least signicant bits
        struct {
            unsigned int a : 1;
            unsigned int b : 10;
            unsigned int c : 20;
            unsigned int d : 1;
        } bits;
    } u;
    
    u.x = 0x00000000;
    u.bits.a = 1;
    printf("After changing a: 0x%08x\n", u.x);
    u.x = 0x00000000;
    u.bits.b = 1;
    printf("After changing b: 0x%08x\n", u.x);
    u.x = 0x00000000;
    u.bits.c = 1;
    printf("After changing c: 0x%08x\n", u.x);
    u.x = 0x00000000;
    u.bits.d = 1;
    printf("After changing d: 0x%08x\n", u.x);
    
    return 0;
}


