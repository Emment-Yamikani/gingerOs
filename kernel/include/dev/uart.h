#ifndef UART_H
#define UART_H

extern volatile int use_uart;

int uart_mount();
void uart_init();
int uart_dev_init();
int uart_tx_empty();
int uart_puts(char *s);
int uart_putc(char c);

#endif //UART_H