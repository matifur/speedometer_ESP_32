#ifndef UART_H_
#define UART_H_

void uart_init(void);
void uart_print(const char *msg);
void uart_printf(const char *fmt, ...);

#endif //UART_H_