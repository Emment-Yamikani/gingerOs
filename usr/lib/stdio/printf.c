#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <ctype.h> //isdigit
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h> //strcpy, strcat, memcpy, memset
#include <stdint.h>
#include <locking/spinlock.h>

static spinlock_t *vdprintf_lock = SPINLOCK_NEW("vdprintf-lock");

#define __fallthrough __attribute__((fallthrough));

char *__int_str(intmax_t i, char b[], int base, bool plusSignIfNeeded, bool spaceSignIfNeeded,
				int paddingNo, bool justify, bool zeroPad)
{
	char digit[32] = {0};
	memset(digit, 0, 32);
	strcpy(digit, "0123456789");

	if (base == 16)
	{
		strcat(digit, "ABCDEF");
	}
	else if (base == 17)
	{
		strcat(digit, "abcdef");
		base = 16;
	}

	char *p = b;
	if (i < 0)
	{
		*p++ = '-';
		i *= -1;
	}
	else if (plusSignIfNeeded)
	{
		*p++ = '+';
	}
	else if (!plusSignIfNeeded && spaceSignIfNeeded)
	{
		*p++ = ' ';
	}

	intmax_t shifter = i;
	do
	{
		++p;
		shifter = shifter / base;
	} while (shifter);

	*p = '\0';
	do
	{
		*--p = digit[i % base];
		i = i / base;
	} while (i);

	int padding = paddingNo - (int)strlen(b);
	if (padding < 0)
		padding = 0;

	if (justify)
	{
		while (padding--)
		{
			if (zeroPad)
			{
				b[strlen(b)] = '0';
			}
			else
			{
				b[strlen(b)] = ' ';
			}
		}
	}
	else
	{
		char a[256] = {0};
		while (padding--)
		{
			if (zeroPad)
			{
				a[strlen(a)] = '0';
			}
			else
			{
				a[strlen(a)] = ' ';
			}
		}
		strcat(a, b);
		strcpy(b, a);
	}

	return b;
}

static int dputc(int fd, int c, int *n)
{
	write(fd, &c, 1);
	*n += 1;
	return 1;
}

static int dputs(int fd, char *s, int *n)
{
	int ret;
	for (ret = 0; *s; s++, ++ret)
		dputc(fd, (int)*s, n);
	return ret;
}

int vdprintf(int fd, const char *format, va_list list)
{
	int chars = 0;
	char intStrBuffer[256] = {0};

	spin_lock(vdprintf_lock);

	for (int i = 0; format[i]; ++i)
	{

		char specifier = '\0';
		char length = '\0';

		int lengthSpec = 0;
		int precSpec = 0;
		bool leftJustify = false;
		bool zeroPad = false;
		bool spaceNoSign = false;
		bool altForm = false;
		bool plusSign = false;
		bool emode = false;
		int expo = 0;

		if (format[i] == '%')
		{
			++i;

			bool extBreak = false;
			while (1)
			{

				switch (format[i])
				{
				case '-':
					leftJustify = true;
					++i;
					break;

				case '+':
					plusSign = true;
					++i;
					break;

				case '#':
					altForm = true;
					++i;
					break;

				case ' ':
					spaceNoSign = true;
					++i;
					break;

				case '0':
					zeroPad = true;
					++i;
					break;

				default:
					extBreak = true;
					break;
				}

				if (extBreak)
					break;
			}

			while (isdigit(format[i]))
			{
				lengthSpec *= 10;
				lengthSpec += format[i] - 48;
				++i;
			}

			if (format[i] == '*')
			{
				lengthSpec = va_arg(list, int);
				++i;
			}

			if (format[i] == '.')
			{
				++i;
				while (isdigit(format[i]))
				{
					precSpec *= 10;
					precSpec += format[i] - 48;
					++i;
				}

				if (format[i] == '*')
				{
					precSpec = va_arg(list, int);
					++i;
				}
			}
			else
			{
				precSpec = 6;
			}

			if (format[i] == 'h' || format[i] == 'l' || format[i] == 'j' ||
				format[i] == 'z' || format[i] == 't' || format[i] == 'L')
			{
				length = format[i];
				++i;
				if (format[i] == 'h')
				{
					length = 'H';
				}
				else if (format[i] == 'l')
				{
					length = 'q';
					++i;
				}
			}
			specifier = format[i];

			memset(intStrBuffer, 0, 256);

			int base = 10;
			if (specifier == 'b')
			{
				base = 2;
				length = 'l';
			}
			if (specifier == 'o')
			{
				base = 8;
				specifier = 'u';
				if (altForm)
				{
					dputs(fd, "0", &chars);
				}
			}
			if (specifier == 'p')
			{
				base = 16;
				length = 'z';
				specifier = 'u';
			}
			switch (specifier)
			{
			case 'b':
				base = 2;
				switch (length)
				{
				case 0:
				{
					unsigned int integer = va_arg(list, unsigned int);
					__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
					dputs(fd, intStrBuffer, &chars);
					break;
				}
				case 'H':
				{
					unsigned char integer = (unsigned char)va_arg(list, unsigned int);
					__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
					dputs(fd, intStrBuffer, &chars);
					break;
				}
				case 'h':
				{
					unsigned short int integer = va_arg(list, unsigned int);
					__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
					dputs(fd, intStrBuffer, &chars);
					break;
				}
				case 'l':
				{
					unsigned long integer = va_arg(list, unsigned long);
					__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
					dputs(fd, intStrBuffer, &chars);
					break;
				}
				case 'q':
				{
					unsigned long long integer = va_arg(list, unsigned long long);
					__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
					dputs(fd, intStrBuffer, &chars);
					break;
				}
				}
				break;
			case 'X':
				base = 16;
				__fallthrough case 'x' : base = base == 10 ? 17 : base;
				if (altForm)
				{
					dputs(fd, "0x", &chars);
				}
				__fallthrough

					case 'u':
				{
					switch (length)
					{
					case 0:
					{
						unsigned int integer = va_arg(list, unsigned int);
						__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
						dputs(fd, intStrBuffer, &chars);
						break;
					}
					case 'H':
					{
						unsigned char integer = (unsigned char)va_arg(list, unsigned int);
						__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
						dputs(fd, intStrBuffer, &chars);
						break;
					}
					case 'h':
					{
						unsigned short int integer = va_arg(list, unsigned int);
						__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
						dputs(fd, intStrBuffer, &chars);
						break;
					}
					case 'l':
					{
						unsigned long integer = va_arg(list, unsigned long);
						__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
						dputs(fd, intStrBuffer, &chars);
						break;
					}
					case 'q':
					{
						unsigned long long integer = va_arg(list, unsigned long long);
						__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
						dputs(fd, intStrBuffer, &chars);
						break;
					}
					case 'j':
					{
						uintmax_t integer = va_arg(list, uintmax_t);
						__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
						dputs(fd, intStrBuffer, &chars);
						break;
					}
					case 'z':
					{
						size_t integer = va_arg(list, size_t);
						__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
						dputs(fd, intStrBuffer, &chars);
						break;
					}
					case 't':
					{
						ptrdiff_t integer = va_arg(list, ptrdiff_t);
						__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
						dputs(fd, intStrBuffer, &chars);
						break;
					}
					default:
						break;
					}
					break;
				}

			case 'd':
			case 'i':
			{
				switch (length)
				{
				case 0:
				{
					int integer = va_arg(list, int);
					__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
					dputs(fd, intStrBuffer, &chars);
					break;
				}
				case 'H':
				{
					signed char integer = (signed char)va_arg(list, int);
					__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
					dputs(fd, intStrBuffer, &chars);
					break;
				}
				case 'h':
				{
					short int integer = va_arg(list, int);
					__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
					dputs(fd, intStrBuffer, &chars);
					break;
				}
				case 'l':
				{
					long integer = va_arg(list, long);
					__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
					dputs(fd, intStrBuffer, &chars);
					break;
				}
				case 'q':
				{
					long long integer = va_arg(list, long long);
					__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
					dputs(fd, intStrBuffer, &chars);
					break;
				}
				case 'j':
				{
					intmax_t integer = va_arg(list, intmax_t);
					__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
					dputs(fd, intStrBuffer, &chars);
					break;
				}
				case 'z':
				{
					size_t integer = va_arg(list, size_t);
					__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
					dputs(fd, intStrBuffer, &chars);
					break;
				}
				case 't':
				{
					ptrdiff_t integer = va_arg(list, ptrdiff_t);
					__int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
					dputs(fd, intStrBuffer, &chars);
					break;
				}
				default:
					break;
				}
				break;
			}

			case 'c':
			{
				if (length == 'l')
				{
					dputc(fd, va_arg(list, int), &chars);
				}
				else
				{
					dputc(fd, va_arg(list, int), &chars);
				}

				break;
			}

			case 's':
			{
				dputs(fd, va_arg(list, char *), &chars);
				break;
			}

			case 'n':
			{
				switch (length)
				{
				case 'H':
					*(va_arg(list, signed char *)) = chars;
					break;
				case 'h':
					*(va_arg(list, short int *)) = chars;
					break;

				case 0:
				{
					int *a = va_arg(list, int *);
					*a = chars;
					break;
				}

				case 'l':
					*(va_arg(list, long *)) = chars;
					break;
				case 'q':
					*(va_arg(list, long long *)) = chars;
					break;
				case 'j':
					*(va_arg(list, intmax_t *)) = chars;
					break;
				case 'z':
					*(va_arg(list, size_t *)) = chars;
					break;
				case 't':
					*(va_arg(list, ptrdiff_t *)) = chars;
					break;
				default:
					break;
				}
				break;
			}

			case 'e':
			case 'E':
				emode = true;
				__fallthrough case 'f' : case 'F' : case 'g' : case 'G':
				{
					double floating = va_arg(list, double);

					while (emode && floating >= 10)
					{
						floating /= 10;
						++expo;
					}

					int form = lengthSpec - precSpec - expo - (precSpec || altForm ? 1 : 0);
					if (emode)
					{
						form -= 4; // 'e+00'
					}
					if (form < 0)
					{
						form = 0;
					}

					__int_str(floating, intStrBuffer, base, plusSign, spaceNoSign, form,
							  leftJustify, zeroPad);

					dputs(fd, intStrBuffer, &chars);

					floating -= (int)floating;

					for (int i = 0; i < precSpec; ++i)
					{
						floating *= 10;
					}
					intmax_t decPlaces = (intmax_t)(floating + 0.5);

					if (precSpec)
					{
						dputc(fd, '.', &chars);
						__int_str(decPlaces, intStrBuffer, 10, false, false, 0, false, false);
						intStrBuffer[precSpec] = 0;
						dputs(fd, intStrBuffer, &chars);
					}
					else if (altForm)
					{
						dputc(fd, '.', &chars);
					}

					break;
				}

			case 'a':
			case 'A':
				//ACK! Hexadecimal floating points...
				break;

			default:
				break;
			}

			if (specifier == 'e')
			{
				dputs(fd, "e+", &chars);
			}
			else if (specifier == 'E')
			{
				dputs(fd, "E+", &chars);
			}

			if (specifier == 'e' || specifier == 'E')
			{
				__int_str(expo, intStrBuffer, 10, false, false, 2, false, true);
				dputs(fd, intStrBuffer, &chars);
			}
		}
		else
		{
			dputc(fd, format[i], &chars);
		}
	}

	spin_unlock(vdprintf_lock);
	return chars;
}

int vprintf(const char *format, va_list list)
{
	return vdprintf(STDOUT_FILENO, format, list);
}

int dprintf(int fd, char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int ret = vdprintf(fd, fmt, args);
	va_end(args);
	return ret;
}

int printf(const char *restrict fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int ret = vprintf(fmt, args);
	va_end(args);
	return ret;
}

void panic(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	exit(-1);
}