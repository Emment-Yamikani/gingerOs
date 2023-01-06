#ifndef CTYPE_H
#define CTYPE_H 1

//for single chars
//is char a digit
extern int isdigit(int c);
//is char a an alphabetic char
extern int isalpha(int c);
//is char uppercase
extern int isupper(int c);
//is char lowercase
extern int islower(int c);
//convert char to uppercase
extern int toupper(int c);
//convert char to lowercase
extern int tolower(int c);
//is char a space(0x20)
extern int isspace(int c);
//convert string to lowercase
extern char *tolowerstr(const char str0[]);
//convert string to uppercase
extern char *toupperstr(const char str0[]);
extern char *strtoupper(char *dest, const char *str0);
//is char an alphabetic char
#define isletter isalpha
void itostr(char *str, int n, int base);

int atoi(const char *);
int atoo(const char *s);

#endif