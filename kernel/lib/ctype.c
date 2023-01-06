#include <lib/ascii.h>
#include <lib/stddef.h>
#include <lib/stdbool.h>
#include <lib/string.h>


int isdigit(int ch)
{
    if (((ch >= ASCII_ZERO)) && ((ch <= ASCII_NINE)))
        return (true);
    else
    {
        return (false);
    }
}

void itostr(char *str, long n, int base)
{
    int sign =0;
    char *digits= "0123456789ABCDEF";


    if(n < 0){
        *str = '-';
        str++;
        sign =1;
    }

    if(sign)
         n *=-1;

    int shifter = n;

    do
    {
        str++;
        shifter /= base;
    }while(shifter);

    *str = '\0';

    do
    {
        --str;
        *str = digits[ n % base];
        n /= base;
    } while (n);
    
}

int atoi(const char *s)
{
    int val =0;
    char *p = (char *)s;
    if (!s)
        return 0;

    while (*p && (*p <= '9'))
    {
        val *= 10;
        val += *p++ - '0';
    }

    return val;
}

int atoo(const char *s)
{
    int val = 0;
    char *p = (char *)s;
    if (!s)
        return 0;

    while (*p && (*p < '8'))
    {
        val <<= 3;
        val |= *p++ - '0';
    }

    return val;
}