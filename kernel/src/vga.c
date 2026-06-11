/* vga.c — VGA text mode driver. See vga.h for how text mode works. */
#include "vga.h"
#include "io.h"
#include "string.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

/* The screen, as the hardware sees it: 80*25 little-endian uint16 cells. */
static volatile uint16_t *const VGA = (volatile uint16_t *)0xB8000;

static int     cur_row;     /* cursor position */
static int     cur_col;
static uint8_t cur_color;   /* current fg/bg attribute byte */

/* Combine a character and the attribute byte into one screen cell. */
static uint16_t cell(char c, uint8_t color)
{
    return (uint16_t)c | ((uint16_t)color << 8);
}

void vga_set_color(enum vga_color fg, enum vga_color bg)
{
    cur_color = (uint8_t)fg | ((uint8_t)bg << 4);
}

/*
 * Move the blinking hardware cursor to (cur_row, cur_col). The VGA chip is
 * controlled through two I/O ports: write a register index to 0x3D4, then
 * the value to 0x3D5. Registers 14/15 are the cursor position hi/lo bytes.
 */
static void update_hw_cursor(void)
{
    uint16_t pos = (uint16_t)(cur_row * VGA_WIDTH + cur_col);
    outb(0x3D4, 14);
    outb(0x3D5, (uint8_t)(pos >> 8));
    outb(0x3D4, 15);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
}

void vga_clear(void)
{
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA[i] = cell(' ', cur_color);
    cur_row = 0;
    cur_col = 0;
    update_hw_cursor();
}

void vga_init(void)
{
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
}

/* When the bottom line fills up: copy every line one row up, blank the last. */
static void scroll(void)
{
    for (int row = 1; row < VGA_HEIGHT; row++)
        for (int col = 0; col < VGA_WIDTH; col++)
            VGA[(row - 1) * VGA_WIDTH + col] = VGA[row * VGA_WIDTH + col];

    for (int col = 0; col < VGA_WIDTH; col++)
        VGA[(VGA_HEIGHT - 1) * VGA_WIDTH + col] = cell(' ', cur_color);

    cur_row = VGA_HEIGHT - 1;
}

void vga_putc(char c)
{
    if (c == '\n') {                       /* newline */
        cur_col = 0;
        cur_row++;
    } else if (c == '\b') {                /* backspace: erase previous cell */
        if (cur_col > 0) {
            cur_col--;
            VGA[cur_row * VGA_WIDTH + cur_col] = cell(' ', cur_color);
        }
    } else {                               /* normal printable character */
        VGA[cur_row * VGA_WIDTH + cur_col] = cell(c, cur_color);
        cur_col++;
        if (cur_col >= VGA_WIDTH) {        /* wrap long lines */
            cur_col = 0;
            cur_row++;
        }
    }

    if (cur_row >= VGA_HEIGHT)
        scroll();
    update_hw_cursor();
}

void vga_print(const char *s)
{
    while (*s)
        vga_putc(*s++);
}

void vga_print_hex(uint32_t value)
{
    char buf[12];
    kitoa(value, buf, 16);
    vga_print("0x");
    vga_print(buf);
}

void vga_print_dec(uint32_t value)
{
    char buf[12];
    kitoa(value, buf, 10);
    vga_print(buf);
}
