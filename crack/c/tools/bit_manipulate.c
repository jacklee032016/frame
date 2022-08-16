#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/*
* big-endian is same as network byte order: MSB is in low address, and is sent first
*/

int check_little_endian(void)
{
    int x = 0x67452301;
    char *c = (char *)&x;

    if(*c == 0x01)
    {// little endian: save small part in low address
        printf("LittleEndian: 0x%x\n", *c);
        printf("%p: 0x%p %p %p %p\n", &x, c, c+1, c+2, c+3);
        printf("%08x: 0x%02x %02x %02x %02x\n\n", x, *c, *(c+1), *(c+2), *(c+3));
        return 0;
    }
    else
    {// big-endian: save big part in low address
        printf("BigEndian: 0x%x\n", *c);
        printf("\n");
        return 1;
    }
}

int check_little_endian2(void)
{
    union Endian
    {
        int x;
        struct 
        {
            char a; // high address
            char b;
            char c;
            char d; // low address
        }y;
    };
        
    union Endian end;
    end.x = 0x01;

    if(end.y.a == 0x01)
    {// little-endian: high address (a) save small part
        printf("LittleEndian: 0x%x\n", end.y.a);
        return 0;
    }
    else
    {
        printf("BigEndian: 0x%x\n", end.y.a);
        return 1;
    }

    printf("\n");
}

#define SWAP(x)     ((((x)&0xFF)<<24)|(((x)&0xFF000000)>>24) | \
                        (((x)&0xFF00)<<8) | (((x)&0xFF0000)>>8) )


int revunion_fields_endian(void)
{
    union u_field{
        int x;

        // align with byte border
        // in little endian system
        struct 
        {
            int a: 4; // a,b,c are the high address
            int b: 2;
            int c: 2;

            int d: 16; 
            int e: 8;  // e is low address, same big part in little-endian system
        }y;
    };

    union u_field2{
        int x;

        // in little endian system
        struct 
        {
            int a: 1; // a,b,c high addresss; save small part in little-endian system
            int b: 10;
            int c: 20;

            int d: 1; 
        }y;
    };

    union u_field uf;
    uf.x = 0;

    uf.y.a = 0xA; // high address,small part
    uf.y.b = 0x2;
    uf.y.c = 0x2;

    uf.y.d = 0x0505;

    uf.y.e = 0x33;

    if(uf.x != 0x330505aa)
    {
        printf("Endian setting is wrong!!\n");
    }
    printf("int: 0x%08x\n", uf.x);

    union u_field2 uf2;

    uf2.x = 0;
    uf2.y.a = 1;
    printf("int after change a: 0x%08x\n", uf2.x);

    uf2.x = 0;
    uf2.y.b = 0x1;
    printf("int after change b: 0x%08x\n", uf2.x);

    uf2.x = 0;
    uf2.y.c = 0x1;
    printf("int after change d: 0x%08x\n", uf2.x);

    uf2.x = 0;
    uf2.y.d = 1;
    printf("int after change d: 0x%08x\n", uf2.x);

    printf("\n");
    return 0;
}

/*
* v&(-v): get lowest set bit
* for example, v=8, lowest set bit is 8; v=9, lowest set bit is 1
* return the number which the least bit is setting
*/
int get_lowest_set_bit(int v)
{
#if 1
    return v&(-v);
#else
    int tmp = v&(-v);
    return log2(tmp&(tmp-1))+1;
//    tmp &= ~tmp + 1;
//    return tmp;
#endif
}


/*
* Brian Kernighan’s Algorithm: v&(v-1)
* v-1 only affects the most-right bit 1 of v
* so v&(v-1) will clean the most-right bit 1 of v
* v=v&(v-1), then recurse this algorithm, only k times can count the k bits in v
*/
// count the number of bit 1 in v
// v&(v-1): awlays clean the MSB 1 in v
int count_bits(int v)
{
    int count = 0;
    
    while(v)
    {
        v = v&(v-1);
        count++;
    }

    return count;
}

int count_bits2(int v)
{
    if(v==0) // end the recurse operation
        return 0;
    
    return (v&0x01) + count_bits2(v>>1);
//    return 0;
}

#define     SET_BIT(v, n)       ((v)|(1<<n))
#define     CLEAN_BIT(v, n)     ((v)&(~(1<<n)))

// clean n lsb bits in v
int clean_lsb_bits(uint32_t v, int i)
{
    // border: i=1, shift left 1, and minus 1, so bit0 is 0; it will clean bit0
    int mask = (1<<(i))-1; // lowest i bits are all 1
    v = (v)&(~mask);

    return v;
}

// clean i msb bits
int clean_msb_bits(uint32_t v, int i)
{
    int count = sizeof(uint32_t)*8;
//    printf("count:%d\n", count);
    int mask = (1<<(count-i))-1; // 32-i bits are 1
    v = (v)&mask;

    return v;
}

uint32_t reverse_bits(uint32_t v)
{
    int count = sizeof(uint32_t)*8;
    uint32_t tmp = 0;
    int i = 0;

    while(v)
    {
        if( (v&0x01) )
        {
            tmp |= 1<<(count-i-1);
        }
        i++;
        v = v>>1;
    }

    return tmp;
}

int main()
{
    check_little_endian();
    check_little_endian2();

    int x = 0x01234567;
    int y = SWAP(x);

    printf("raw:0x%08X; swapped:0x%08X\n\n", x, y);

    union_fields_endian();

    printf("Count bits:\n");
    x = 0x08;
    printf("\tcount bits of 0x%08x: %d, %d\n", x, count_bits(x), count_bits2(x));
    x = 0x09;
    printf("\tcount bits of 0x%08x: %d, %d\n", x, count_bits(x), count_bits2(x));

    x = 0x9999;
    printf("\tcount bits of 0x%08x: %d, %d\n", x, count_bits(x), count_bits2(x));
    printf("\n");

    printf("Lowest set bit:\n");
    x = 0x08;
    printf("\tlowest set bit of 0x%08x: %d\n", x, get_lowest_set_bit(x));
    x = 0x09;
    printf("\tlowest set bit of 0x%08x: %d\n", x, get_lowest_set_bit(x));
    printf("\n");
    
    printf("set/clean bit:\n");
    x = 0x00;
    printf("\tset bit of 0x%08x: 0x%08x\n", x, SET_BIT(x, 3));
    printf("\tclean bit of 0x%08x: 0x%08x\n", x, CLEAN_BIT(x, 3));
    x = 0xFFFFFFFF;
    printf("\tclean lsb of 0x%08x: 0x%08x\n", x, clean_lsb_bits(x, 5));
    x = 0xFFFFFFFF;
    printf("\tclean msb of 0x%08x: 0x%08x\n", x, clean_msb_bits(x, 5));
    x = 0xFFFFFFFF;
    printf("\tclean msb of 0x%08x: 0x%08x\n", x, clean_msb_bits(x, 16));
    printf("\n");
    
    printf("reverse bits:\n");
    x = 0x80000000;
    printf("\treverse bits of 0x%08x: 0x%08x\n", x, reverse_bits(x));
    x = 0x00000001;
    printf("\treverse bits of 0x%08x: 0x%08x\n", x, reverse_bits(x));
    x = 0xAA55AA55;
    printf("\treverse bits of 0x%08x: 0x%08x\n", x, reverse_bits(x));
    x = 0xAA55DD33;
    printf("\treverse bits of 0x%08x: 0x%08x\n", x, reverse_bits(x));
    printf("\n");

    
    return 0;
}

