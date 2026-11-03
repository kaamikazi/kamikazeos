/* Only port outcomes/logging controlled; execute actual ATA guard and retries. */
#include "../kernel/src/ata.h"
#include "../kernel/src/errors.h"
#define KMZ_IO_H
static uint32_t port_calls,idle_calls,word_index,command;
static int fail,absent;
static void observe(void){port_calls++;if(!ata_io_in_progress())idle_calls++;}
static uint8_t inb(uint16_t port)
{ observe();if(absent)return 0xFF;(void)port;return fail?9:8; }
static void outb(uint16_t port,uint8_t value)
{ observe();if(port==0x1F7){command=value;word_index=0;} }
static uint16_t inw(uint16_t port)
{ observe();(void)port;if(command==0xEC){uint32_t i=word_index++;return i==49?512:i==60?2048:0;}return 0x1234; }
static void outw(uint16_t port,uint16_t value){observe();(void)port;(void)value;}
static void io_wait(void){observe();}
void error_log(enum err_severity s,const char*c,const char*o,const char*m,const char*h)
{(void)s;(void)c;(void)o;(void)m;(void)h;}
#include "../kernel/src/ata.c"
static void setup(void){fail=absent=0;port_calls=idle_calls=0;ata_init();}
int test_ata_guard_covers_initialization_reads_and_writes(void)
{
    setup();if(!ata_available() || ata_io_in_progress() || idle_calls)return 0;
    uint8_t buffer[512];
    return ata_read_sector(1,buffer) && buffer[0]==0x34 && buffer[1]==0x12 &&
        ata_write_sector(1,buffer) && !ata_io_in_progress() && port_calls && !idle_calls;
}
int test_ata_guard_is_released_on_retry_failure_and_invalid_requests(void)
{
    setup();uint8_t buffer[512];fail=1;
    if(ata_read_sector(1,buffer) || ata_write_sector(1,buffer) || ata_io_in_progress() || idle_calls)return 0;
    uint32_t before=port_calls;
    return !ata_read_sector(2048,buffer) && !ata_write_sector(1,0) && port_calls==before && !ata_io_in_progress();
}
int test_absent_disk_initialization_does_not_leave_io_guard_active(void)
{
    setup();absent=1;ata_init();
    return !ata_available() && !ata_io_in_progress() && !idle_calls;
}
