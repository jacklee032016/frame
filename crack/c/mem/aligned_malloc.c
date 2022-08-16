#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#define ALIGN_SIZE  32

#define ALIGN_BORDER(size)      (((size)+ALIGN_SIZE-1)&(~(ALIGN_SIZE-1)))

void* a_malloc(int size)
{
    void *p;
    void **pp;
    int a_size = ALIGN_BORDER(size + sizeof(void *));
    printf("size %d, align size:%d\n", size, a_size);

    p = (void *)malloc(a_size);
    if(!p)
        return NULL;
    
    pp = (void **)ALIGN_BORDER((uintptr_t)p+sizeof(void *));
    *(pp-1) = p;

    assert((uintptr_t)pp%ALIGN_SIZE==0 );
    printf("allocate:%p; return %p\n", p, pp);

    return pp;
}


void a_free(void *p)
{
    void **pp = (void **)p;
    void *ptr = NULL;
    
    ptr = *(pp-1);
    printf("free prt %p; param:%p; \n", ptr, p);

    free(ptr);
}


int main()
{
    int i;
    void *ptr = NULL;

    for(i=0; i<10; i++)
    {
        ptr = a_malloc(i*10+2);
        
        a_free(ptr);
    }

    return 0;
}

