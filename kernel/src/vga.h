/*
 * vga.h — text output to the screen.
 *
 * The classic PC "VGA text mode" gives us an 80×25 character grid that lives
 * directly in memory at physical address 0xB8000. Each cell is 2 bytes:
 * byte 0 = the ASCII character, byte 1 = colors (low 4 bits foreground,
 * high 4 bits background). Writing to that memory instantly changes the
 * screen — no driver needed. This is the simplest possible display "driver"
 * and exactly why many small experimental kernels start here.
 */
#ifndef KMZ_VGA_H
#define KMZ_VGA_H

#include <stdint.h>

/* The 16 standard VGA colors. */
enum vga_color {
    VGA_BLACK = 0,  VGA_BLUE = 1,     VGA_GREEN = 2,      VGA_CYAN = 3,
    VGA_RED = 4,    VGA_MAGENTA = 5,  VGA_BROWN = 6,      VGA_LIGHT_GREY = 7,
    VGA_DARK_GREY = 8, VGA_LIGHT_BLUE = 9, VGA_LIGHT_GREEN = 10,
    VGA_LIGHT_CYAN = 11, VGA_LIGHT_RED = 12, VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW = 14, VGA_WHITE = 15,
};

void vga_init(void);                              /* clear screen, home cursor */
void vga_clear(void);
void vga_set_color(enum vga_color fg, enum vga_color bg);
void vga_putc(char c);                            /* handles \n, \b, scrolling */
void vga_print(const char *s);
void vga_print_hex(uint32_t value);               /* prints e.g. 0x1bad */
void vga_print_dec(uint32_t value);

#endif /* KMZ_VGA_H */
