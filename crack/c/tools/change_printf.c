// C Program to demonstrate the use of macro arguments to change the output of printf()
#include <stdio.h>
 
void fun()
{
#define printf(x, y) printf(x, 10);
}
 
// Driver Code
int main()
{
    int i = 10;
    fun();
    i = 20;
    printf("%d\n", i);
    return 0;
}


