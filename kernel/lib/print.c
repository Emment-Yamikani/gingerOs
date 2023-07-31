#include <lib/string.h>
#include <lib/stdint.h>
#include <lib/stddef.h>
#include <lib/string.h> //strcpy, strcat, memcpy, memset
#include <lib/ctype.h>  //isdigit
#include <lib/stdbool.h>
#include <lib/stdint.h>
#include <lib/stdarg.h>
#include <arch/system.h>
#include <sys/system.h>
#include <dev/uart.h>
#include <printk.h>
#include <dev/cons.h>
#include <arch/boot/early.h>
#include <locks/spinlock.h>
#include <video/lfbterm.h>

static spinlock_t *kvprintf_lk = SPINLOCK_NEW("kvprintf-lock");

//putchar
int putchar(int c)
{
    if (use_early_cons)
        earlycons_putc(c);
    if (use_uart)
        uart_putc(c);
    cons_write((const char *)&c, 1);
    return 1;
}

static void set_color(int bg, int fg)
{
    if (use_early_cons)
        earlycons_setcolor(bg, fg);
    else
    {
        cons_save();
        cons_setcolor(bg, fg);
    }
}

void restore_color(void)
{
    if (use_early_cons)
        earlycons_restore_color();
    else
        cons_restore();
}

int puts(const char *s)
{
    if (use_early_cons)
        return earlycons_puts(s);
    else
        return cons_puts(s);
    return 0;
}


char *__int_str(intmax_t i, char b[], int base, bool plusSignIfNeeded, bool spaceSignIfNeeded,
                int paddingNo, bool justify, bool zeroPad)
{
    char digit[32] = {0};
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

void displayCharacter(char c, int *a)
{
    putchar(c);
    *a += 1;
}

void displayString(char *c, int *a)
{
    for (int i = 0; c[i]; ++i)
    {
        displayCharacter(c[i], a);
    }
}

int kvprintf(const char *format, va_list list)
{
    int locked = 0;
    int chars = 0;
    char intStrBuffer[256] = {0};
    
    if (!(locked = spin_holding(kvprintf_lk)))
        spin_lock(kvprintf_lk);

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
            if (format[i] == '%')
                displayCharacter('%', &chars);
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
                    displayString("0", &chars);
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
                    base =2;
                    switch(length)
                    {
                        case 0:
                        {
                            unsigned int integer = va_arg(list, unsigned int);
                            __int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
                            displayString(intStrBuffer, &chars);
                            break;
                        }
                        case 'H':
                        {
                            unsigned char integer = (unsigned char)va_arg(list, unsigned int);
                            __int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
                            displayString(intStrBuffer, &chars);
                            break;
                        }
                        case 'h':
                        {
                            unsigned short int integer = va_arg(list, unsigned int);
                            __int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
                            displayString(intStrBuffer, &chars);
                            break;
                        }
                        case 'l':
                        {
                            unsigned long integer = va_arg(list, unsigned long);
                            __int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
                            displayString(intStrBuffer, &chars);
                            break;
                        }
                        case 'q':
                        {
                            unsigned long long integer = va_arg(list, unsigned long long);
                            __int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
                            displayString(intStrBuffer, &chars);
                            break;
                        }
                    }
                    break;
            case 'X':
                base = 16;
                __fallthrough;
                case 'x' : base = base == 10 ? 17 : base;
                if (altForm)
                {
                    displayString("0x", &chars);
                }
                __fallthrough;

                    case 'u':
                {
                    switch (length)
                    {
                    case 0:
                    {
                        unsigned int integer = va_arg(list, unsigned int);
                        __int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
                        displayString(intStrBuffer, &chars);
                        break;
                    }
                    case 'H':
                    {
                        unsigned char integer = (unsigned char)va_arg(list, unsigned int);
                        __int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
                        displayString(intStrBuffer, &chars);
                        break;
                    }
                    case 'h':
                    {
                        unsigned short int integer = va_arg(list, unsigned int);
                        __int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
                        displayString(intStrBuffer, &chars);
                        break;
                    }
                    case 'l':
                    {
                        unsigned long integer = va_arg(list, unsigned long);
                        __int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
                        displayString(intStrBuffer, &chars);
                        break;
                    }
                    case 'q':
                    {
                        unsigned long long integer = va_arg(list, unsigned long long);
                        __int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
                        displayString(intStrBuffer, &chars);
                        break;
                    }
                    case 'j':
                    {
                        uintmax_t integer = va_arg(list, uintmax_t);
                        __int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
                        displayString(intStrBuffer, &chars);
                        break;
                    }
                    case 'z':
                    {
                        size_t integer = va_arg(list, size_t);
                        __int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
                        displayString(intStrBuffer, &chars);
                        break;
                    }
                    case 't':
                    {
                        ptrdiff_t integer = va_arg(list, ptrdiff_t);
                        __int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
                        displayString(intStrBuffer, &chars);
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
                if (!use_early_cons)
                {
                    // cons_save();
                    // cons_setcolor(CGA_BLACK, CGA_GREEN);
                }

                switch (length)
                {
                case 0:
                {
                    int integer = va_arg(list, int);
                    __int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
                    displayString(intStrBuffer, &chars);
                    break;
                }
                case 'H':
                {
                    signed char integer = (signed char)va_arg(list, int);
                    __int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
                    displayString(intStrBuffer, &chars);
                    break;
                }
                case 'h':
                {
                    short int integer = va_arg(list, int);
                    __int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
                    displayString(intStrBuffer, &chars);
                    break;
                }
                case 'l':
                {
                    long integer = va_arg(list, long);
                    __int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
                    displayString(intStrBuffer, &chars);
                    break;
                }
                case 'q':
                {
                    long long integer = va_arg(list, long long);
                    __int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
                    displayString(intStrBuffer, &chars);
                    break;
                }
                case 'j':
                {
                    intmax_t integer = va_arg(list, intmax_t);
                    __int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
                    displayString(intStrBuffer, &chars);
                    break;
                }
                case 'z':
                {
                    size_t integer = va_arg(list, size_t);
                    __int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
                    displayString(intStrBuffer, &chars);
                    break;
                }
                case 't':
                {
                    ptrdiff_t integer = va_arg(list, ptrdiff_t);
                    __int_str(integer, intStrBuffer, base, plusSign, spaceNoSign, lengthSpec, leftJustify, zeroPad);
                    displayString(intStrBuffer, &chars);
                    break;
                }
                default:
                    break;
                }
                
                if (!use_early_cons)
                {
                    // cons_restore();
                }
                break;
            }


            case 'c':
            {
                if (length == 'l')
                {
                    displayCharacter(va_arg(list, int), &chars);
                }
                else
                {
                    displayCharacter(va_arg(list, int), &chars);
                }

                break;
            }

            case 's':
            {
                if (!use_early_cons)
                {            
                    // cons_save();
                    // cons_setcolor(CGA_BLACK, CGA_BROWN);
                }
                
                char *s = va_arg(list, char *);
                int pad = lengthSpec - strlen(s);
                if(pad < 0)
                    pad = 0;
                while(pad--)
                    displayCharacter(' ', &chars);
                if (s)
                    displayString(s, &chars);
                else
                    displayString("(null)", &chars);
                
                if (!use_early_cons)
                {
                    // cons_restore();
                }
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
                __fallthrough;
                case 'f' : case 'F' : case 'g' : case 'G':
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

                    displayString(intStrBuffer, &chars);

                    floating -= (int)floating;

                    for (int i = 0; i < precSpec; ++i)
                    {
                        floating *= 10;
                    }
                    intmax_t decPlaces = (intmax_t)(floating + 0.5);

                    if (precSpec)
                    {
                        displayCharacter('.', &chars);
                        __int_str(decPlaces, intStrBuffer, 10, false, false, 0, false, false);
                        intStrBuffer[precSpec] = 0;
                        displayString(intStrBuffer, &chars);
                    }
                    else if (altForm)
                    {
                        displayCharacter('.', &chars);
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
                displayString("e+", &chars);
            }
            else if (specifier == 'E')
            {
                displayString("E+", &chars);
            }

            if (specifier == 'e' || specifier == 'E')
            {
                __int_str(expo, intStrBuffer, 10, false, false, 2, false, true);
                displayString(intStrBuffer, &chars);
            }
        }
        else if (format[i] == '\e')
        {
            __unused int pos = 0;
            __unused int len = 0;
            __unused int color =0;
            __unused char bg[9]= {0};
            __unused char fg[9]= {0};
            __unused int fore = 0;
            __unused int back = 0;

            i++;
            if (format[i++] != '[')
                break;
            while (len++ < 20)
            {
                while ((format[i] != ';'))
                {
                    if ((format[i] == 'm'))
                        break;
                    bg[pos++] = format[i++];
                }

                if (format[i] == ';')
                    color = 1;
                else if (format[i] == 'm')
                    goto set;
                else
                    break;
                i++;
                pos = 0;
                while (format[i] != 'm')
                    fg[pos++] = format[i++];

                back = atoo(bg);
                fore = atoo(fg);

                set:
                if (!use_early_cons)
                {
                    
                    if ((!color && !back))
                        restore_color();
                    else if (color)
                        set_color(back, fore);
                }
                break;
            }
        }
        else
        {
            displayCharacter(format[i], &chars);
        }
    }

    if (!locked)
        spin_unlock(kvprintf_lk);
    return chars;
}

int printk(const char * restrict format, ...)
{
    spin_lock(kvprintf_lk);
    int chars =0;
    va_list list;
    va_start(list, format);
    chars = kvprintf(format, list);
    va_end(list);
    spin_unlock(kvprintf_lk);
    return chars;
}

volatile int panicked = 0;

__noreturn void ginger_death_loop(void)
{
    cli();
    /**
     * just in case othe cpus are not aware of the panic,
     * call them all here,
     * maybe even implement a kernel based debugger ???.
     * and maybe even enhance this function
     * by using an IPI instead of setting 'panicked' to call other cpus???
     **/
    __atomic_exchange_n(&panicked, 1, __ATOMIC_SEQ_CST);
    for (;; hlt())
        ;
}


int kvlogf(int type, const char *restrict fmt, va_list list)
{
    int chars =0;

    switch (type)
    {
    case KLOG_SILENT:
        chars = kvprintf(fmt, list);
        break;
    case KLOG_OK:
        chars = puts("[ ");
        set_color(CGA_BLACK, CGA_LIGHT_GREEN);
        chars += puts("OK");
        restore_color();
        chars += puts(" ] ");
        chars += kvprintf(fmt, list);
        break;
    case KLOG_INIT:
        chars = puts("[ ");
        set_color(CGA_BLACK, CGA_LIGHT_CYAN);
        chars += puts("INIT");
        restore_color();
        chars += puts(" ] ");
        chars += kvprintf(fmt, list);
        break;
    case KLOG_FAIL:
        chars = puts("[ ");
        set_color(CGA_BLACK, CGA_RED);
        chars += puts("FAILED");
        restore_color();
        chars += puts(" ] ");
        chars += kvprintf(fmt, list);
        break;
    case KLOG_WARN:
        chars = puts("[ ");
        set_color(CGA_BLACK, CGA_YELLOW);
        chars += puts("WARNING");
        restore_color();
        chars += puts(" ] ");
        chars += kvprintf(fmt, list);
        break;
    case KLOG_PANIC:
        chars = puts("[ ");
        set_color(CGA_BLACK, CGA_LIGHT_RED);
        chars += puts("PANIC");
        restore_color();
        chars += puts(" ] ");
        chars += kvprintf(fmt, list);
        if (spin_holding(kvprintf_lk))  // prevent deadlock
            spin_unlock(kvprintf_lk);
        ginger_death_loop();
    }

    return chars;
}

void panic(const char * restrict format, ...)
{
    va_list list;
    cli();
    __atomic_exchange_n(&panicked, 1, __ATOMIC_SEQ_CST);
    va_start(list, format);
    spin_lock(kvprintf_lk);
    kvlogf(KLOG_PANIC, format, list);
    spin_unlock(kvprintf_lk);
    va_end(list);
}

int klog(int type, const char *restrict fmt, ...)
{
    int chars =0;
    va_list list;
    va_start(list, fmt);
    
    spin_lock(kvprintf_lk);
    chars = kvlogf(type, fmt, list);
    spin_unlock(kvprintf_lk);

    va_end(list);
    return chars;
}

int klog_init(int type, const char *restrict message)
{
    int chars =0;
    
    switch(type)
    {
    case KLOG_INIT:
        chars = klog(type, "initializing \'%s\'...\n", message);
        break;
    case KLOG_OK:
        chars = klog(type, "\'%s\' initalized.\n", message);
        break;
    case KLOG_FAIL:
        chars = klog(type, "\'%s\' not initialized\n", message);
        break;
    }
    return chars;
}

int early_printf(const char *restrict fmt, ...)
{
    int chars = 0;
    va_list list;
    va_start(list, fmt);
    chars = kvprintf(fmt, list);
    va_end(list);
    return chars;
}