#ifndef CONSOLE_H
#define CONSOLE_H 1

#define VDISABLE CTRL('\0')

#define CR      CTRL('\r')
#define DISCARD CTRL('O')
#define DSUSP   CTRL('Y')
#define EOF     CTRL('D')
#define EOL     CTRL('L')
#define EOL2    VDISABLE //disabled by default CTRL('L')
#define ERASE   CTRL('H')
#define ERASE2  VDISABLE //disabled by default CTRL('?')
#define INTR    CTRL('C')
#define KILL    CTRL('U')
#define LNEXT   CTRL('V')
#define NL      CTRL('\n')
#define QUIT    CTRL('\\')
#define REPRINT CTRL('R')
#define START   CTRL('Q')
#define STOP    CTRL('S')
#define SUSP    CTRL('Z')
#define WERASE  CTRL('W')
#define STATUS  CTRL('T')

#define VCR      0
#define VDISCARD 1
#define VDSUSP   2
#define VEOF     3
#define VEOL     4
#define VEOL2    5
#define VERASE   6
#define VERASE2  7
#define VINTR    8
#define VKILL    9
#define VLNEXT   10
#define VNL      11
#define VQUIT    12
#define VREPRINT 13
#define VSTART   14
#define VSTOP    15
#define VSUSP    16
#define VWERASE  17
#define VSTATUS   18


#endif // CONSOLE_H