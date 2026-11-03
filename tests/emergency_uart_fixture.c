/* Mock only port outcomes; execute the actual production UART budgets. */
#include <stdint.h>
#define KMZ_IO_H
static uint32_t reads,writes,ready_after;static int absent;
static char bytes[8192];
static uint8_t inb(uint16_t port)
{ if(port==0x3F8)return absent?0:0xAE;if(port==0x3FD){reads++;return reads>=ready_after?0x20:0;}return 0; }
static void outb(uint16_t port,uint8_t value)
{ if(port==0x3F8){if(writes<sizeof(bytes))bytes[writes]=(char)value;writes++;} }
#include "../kernel/src/serial.c"
static void setup(void){absent=0;ready_after=1;serial_init();reads=writes=0;}
int test_uart_stuck_polling_and_oversized_budgets_are_bounded(void)
{
    setup();ready_after=UINT32_MAX;
    if(serial_putc_bounded('x',7) || reads!=7 || writes)return 0;
    reads=0;if(serial_putc_bounded('x',UINT32_MAX) || reads!=SERIAL_MAX_POLLS || writes)return 0;
    reads=0;serial_putc('x');return reads==SERIAL_MAX_POLLS && !writes;
}
int test_uart_ready_absent_and_zero_budget_paths(void)
{
    setup();ready_after=3;
    if(!serial_putc_bounded('x',3) || reads!=3 || writes!=1)return 0;
    setup();if(serial_putc_bounded('x',0) || reads || writes)return 0;
    absent=1;serial_init();reads=writes=0;
    return !serial_putc_bounded('x',7) && !reads && !writes;
}
int test_uart_strings_and_fields_have_hard_caps(void)
{
    static char input[SERIAL_MAX_STRING+32];
    for(uint32_t i=0;i<sizeof(input);i++)input[i]='A';
    setup();if(serial_print_bounded(input,UINT32_MAX,1)!=SERIAL_MAX_STRING || writes!=SERIAL_MAX_STRING)return 0;
    setup();serial_print_field(input);return writes==SERIAL_MAX_STRING;
}
int test_uart_crlf_and_timeout_string_termination(void)
{
    setup();if(serial_print_bounded("a\n",8,1)!=2 || writes!=3 || bytes[1]!='\r' || bytes[2]!='\n')return 0;
    setup();ready_after=UINT32_MAX;
    return !serial_print_bounded("test",4,9) && reads==9 && !writes;
}
