
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int find_longest_palindrome_string(const char *str)
{
    int i, j, k;
    int size = strlen(str);
    int max_size = 0;
    int index = -1;

    for(i=0; i< size; i++)
    {
        for(j=i; j< size; j++)
        {
            int found = 1;
            int count = j-i+1;
            if (count ==1)
                found = 0;

            for(k=0; k< count/2; k++)
            {
                if(str[i+k] != str[j-k])
                {
                    found = 0;
                    break;
                }
            }

            if (found && count> max_size)
            {
                index = i;
                max_size = count;
            }
        }
    }

    if(index != -1)
    {
        printf("found at %d, size %d: %.*s\n", index, max_size, max_size, str+index);
    }

    return max_size;
}

int main()
{
    char *str = "forgeeksskeegfor";
    int length = find_longest_palindrome_string(str);
    printf("Length is: %d\n", length);

    length = find_longest_palindrome_string("Geeks");
    printf("Length is: %d\n", length);
    return 0;
}

