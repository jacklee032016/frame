#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// sizeof a variable, not support sizeof a type
#define MY_SIZEOF(var)      ((uintptr_t)(&(var)+1) - (uintptr_t)(&(var)))

#define MY_OFFSET(type, field)  ((uintptr_t)&(((type *)0)->field) - (uintptr_t)((type *)0))

#define MY_CONTAINER(var, type, field)        (type *)((uintptr_t)(var) - MY_OFFSET(type, field))

struct test_struct
{
    int     a;
    short   s;
    char    b[13];
    int     d;
};

struct test_packed
{
    int     a;
    short   s;
    char    b[13];
    int     d;
}__attribute__((packed));

int main()
{
    char c;
    short s;
    uint32_t u32;
    int i;
    long l;

    struct test_packed tp;
    char *cp = tp.b;
    
    printf("sizeof:\n");
    printf("\tsizeof: %ld, %ld, %ld, %ld, %ld\n", MY_SIZEOF(c), MY_SIZEOF(s), MY_SIZEOF(u32), MY_SIZEOF(i), MY_SIZEOF(l));
    printf("\n");

    printf("Offset:\n");
    printf("\tOffset: %ld, %ld, %ld, %ld\n", MY_OFFSET(struct test_struct, a), MY_OFFSET(struct test_struct, s), 
         MY_OFFSET(struct test_struct, b), MY_OFFSET(struct test_struct, d));
    printf("\tOffset: %ld, %ld, %ld, %ld\n", MY_OFFSET(struct test_packed, a), MY_OFFSET(struct test_packed, s), 
         MY_OFFSET(struct test_packed, b), MY_OFFSET(struct test_packed, d));
    printf("\n");

    printf("contain:\n");
    printf("\tstart: %p, cp:%p, container:%p\n", &tp, cp, MY_CONTAINER(cp, struct test_packed, b));
    printf("\n");

    return 0;
}


