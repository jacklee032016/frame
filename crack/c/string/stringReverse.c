
#include <stdio.h>
#include <string.h>

void swap(char *a, char *b)
{
#if 1
    *a = (*a)^(*b);
    *b = (*a)^(*b);
    *a = (*a)^(*b);
#else
    char tmp = (char)*a;
    *a = *b;
    *b = tmp;
#endif
}

char* stringReverse(char *str)
{
    char *f, *l;
    f = str;

    l = f+ strlen(str) - 1;

    while( f < l)
    {
        swap(f, l);

        f++;
        l--;
    }

    return str;
}

int main()
{
    char    str[] = "This is a string, need to be reversed";
    char    str2[] = "abcde";
    char    str3[] = "123456";

    printf("raw string: '%s'\n", str);
    stringReverse(str);
    printf("After reversed string: '%s'\n", str);
    
    printf("raw string: '%s'\n", str2);
    stringReverse(str2);
    printf("After reversed string: '%s'\n", str2);
    
    printf("raw string: '%s'\n", str3);
    stringReverse(str3);
    printf("After reversed string: '%s'\n", str3);
    return 0;
}

