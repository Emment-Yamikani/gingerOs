#ifndef G_STRING_H
#define G_STRING_H 1

#include <lib/stdint.h>

// compare two string literals, return 0 if equal or otherwise if not equal
int compare_strings(const char *s0, const char *s1);
// combine two strings and return a pointer to the new string, or null if an error occured
char *combine_strings(const char *s0, const char *s1);

/**
 * @brief concatenate string 'str' and numerical value 'num'
 * 
 * @param str string
 * @param num numerical value
 * @param base of numerical value
 * @return char * 
 */
char *strcat_num(char *str, long num, uint8_t base);

//clear region pointed to by ptr
void ginger_memclear(void *, int);

#endif // G_STRING_H