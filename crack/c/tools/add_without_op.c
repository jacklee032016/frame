#include <stdio.h>
#include <stdint.h>

/*
* half-add theory: one bit number a and b
* carrier = a&b
* s = a^b  XOR, keep all the set bits
* sum = 2xc + s   # carrier left shift one bit
*/

int add(uint32_t a, uint32_t b)
{
    uint32_t carriers = 0;

    
    while(b) // still carriers
    {
        carriers = a&b;
        a = a^b;

        b = carriers<<1;
    }

    return a;
}


int add_recurse(uint32_t a, uint32_t b)
{
    if(b==0)
        return a;
    else
        return add_recurse( (a^b), (a&b)<<1 );
}


int main(int argc, char *argv[])
{
    printf("add result=%d\n", add(10, 20));
    printf("add result=%d\n", add_recurse(25, 20));
    return 0;
}

