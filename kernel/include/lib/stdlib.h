#ifndef CORE_STDLIB
#define CORE_STDLIB 1

#define	RAND_MAX	2147483647

extern int rand(void);
extern void srand(unsigned int __seed);

#endif // CORE_STDLIB