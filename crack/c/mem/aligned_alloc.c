#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

/*
*  ptr&(aligned-1): get m least signicant bits
*  ptr&(~(aligned-1)): get N-m most signicant bits
*
*  usage of pointer to pointer
*/

void* aligned_allocate(int size, int aligned)
{
    void *ptr = NULL;
    void **pptr = NULL;// pointer to pointer, so use [] to access the item in array
    
    int offset_size = 0;
    if( (aligned&(aligned-1))!=0 )
    {
        printf("Aligned must on the border of 2**n, %u is not\n", aligned);
        return NULL;
    }

    offset_size = sizeof(void *)+aligned -1;
    ptr = malloc(offset_size + size);
    if(!ptr)
        return NULL;

    printf("allocate address:%p, length:%u\n", ptr, offset_size + size);
    pptr = (void **)(((uintptr_t)ptr+aligned-1)&(~(aligned-1)));
//    *((void *)pptr[-1]) = ptr;
    pptr[-1] = ptr;

    return pptr;
}


void aligned_free(void *ptr)
{
#if 0
    void **pptr = (void **)ptr;
    void *freeP = NULL;

    freeP = (void *)pptr[-1];

    printf("free address: %p\n", freeP);
    free(freeP);
#else
    free( ((void **)ptr)[-1]);
#endif
}


int main(int argc, char* argv[])
{
    int aligned = 15;
    void *ptr = aligned_allocate(36, aligned);
    assert(ptr==NULL);

    aligned = 32;
    ptr = aligned_allocate(36, aligned);
    printf("aligned address: %p\n", ptr);
    int position = (uintptr_t)ptr;

    assert( (position%aligned)==0); // '~(aligned-1)': get aligned the address
    assert( (position&(aligned-1))==0); // '~(aligned-1)': get aligned the address

    aligned_free(ptr);
    
    return 0;
}

