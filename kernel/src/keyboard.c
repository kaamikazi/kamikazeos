/* keyboard.c — polling PS/2 keyboard driver. See keyboard.h. */
#include "keyboard.h"
#include "errors.h"
#include "io.h"
#include "pic.h"

#define PS2_DATA    0x60   /* read scancodes here                    */
#define PS2_STATUS  0x64   /* bit 0 set = a byte is waiting in DATA  */

static int present;        /* did the controller answer at boot?     */
static int shift_down;     /* live state of the Shift keys           */
static int irq_mode;

#define KEY_BUFFER_SIZE 64
static volatile char key_buffer[KEY_BUFFER_SIZE];
static volatile uint8_t key_head;
static volatile uint8_t key_tail;

/*
 * Scancode set 1 -> ASCII lookup tables (US layout).
 * The scancode is the table index. 0 = key we don't handle in Phase 1.
 * Index range 0x00..0x39 covers the whole main typing area.
 */
static const char map_lower[] = {
      0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
      0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
      0,'\\','z','x','c','v','b','n','m',',','.','/',  0,
    '*',  0, ' ',
};

static const char map_upper[] = {
      0,  27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
      0, 'A','S','D','F','G','H','J','K','L',':','"','~',
      0, '|','Z','X','C','V','B','N','M','<','>','?',  0,
    '*',  0, ' ',
};

#define SC_LSHIFT_DOWN 0x2A
#define SC_RSHIFT_DOWN 0x36
#define SC_RELEASED    0x80   /* bit 7 set = this is a key RELEASE event */

static char translate_scancode(uint8_t sc)
{
    if (sc == SC_LSHIFT_DOWN || sc == SC_RSHIFT_DOWN) {
        shift_down = 1;
        return 0;
    }
    if (sc == (SC_LSHIFT_DOWN | SC_RELEASED) ||
        sc == (SC_RSHIFT_DOWN | SC_RELEASED)) {
        shift_down = 0;
        return 0;
    }
    if (sc & SC_RELEASED)
        return 0;
    if (sc < sizeof(map_lower))
        return shift_down ? map_upper[sc] : map_lower[sc];
    return 0;
}

static void buffer_put(char c)
{
    uint8_t next = (uint8_t)((key_head + 1u) % KEY_BUFFER_SIZE);
    if (next == key_tail)
        return;
    key_buffer[key_head] = c;
    key_head = next;
}

void keyboard_init(void)
{
    irq_mode = 0;
    key_head = key_tail = 0;
    shift_down = 0;
    /*
     * Light-touch probe: flush any stale bytes out of the controller's
     * output buffer. If the status port reads 0xFF, no controller is
     * there at all (floating bus). QEMU always has one, but GhostAttach
     * thinking starts now: detect, don't assume.
     */
    if (inb(PS2_STATUS) == 0xFF) {
        present = 0;
        error_log(SEV_ERROR, "KBD-001", "keyboard",
                  "No PS/2 controller detected; keyboard input unavailable",
                  "check emulator input configuration");
        return;
    }
    while (inb(PS2_STATUS) & 1)
        (void)inb(PS2_DATA);

    present = 1;
    error_log(SEV_INFO, "KBD-000", "keyboard",
              "PS/2 keyboard attached", "IRQ mode will be attempted");
}

void keyboard_enable_irq(void)
{
    if (!present)
        return;
    pic_enable_irq(1);
    irq_mode = pic_irq_enabled(1);
    if (irq_mode)
        error_log(SEV_INFO, "KBD-IRQ-000", "keyboard",
                  "PS/2 keyboard IRQ mode enabled", "polling fallback remains available");
}

void keyboard_handle_irq(void)
{
    if (!present || (inb(PS2_STATUS) & 1) == 0)
        return;
    char c = translate_scancode(inb(PS2_DATA));
    if (c)
        buffer_put(c);
}

int keyboard_present(void)
{
    return present;
}

int keyboard_irq_enabled(void)
{
    return irq_mode;
}

const char *keyboard_mode_name(void)
{
    return irq_mode ? "irq" : "polling_fallback";
}

char keyboard_getchar(void)
{
    if (irq_mode) {
        for (;;) {
            __asm__ volatile ("cli");
            if (key_head != key_tail) {
                char c = key_buffer[key_tail];
                key_tail = (uint8_t)((key_tail + 1u) % KEY_BUFFER_SIZE);
                __asm__ volatile ("sti");
                return c;
            }
            __asm__ volatile ("sti; hlt");
        }
    }

    for (;;) {
        /* Wait until the controller has a byte for us (status bit 0). */
        while ((inb(PS2_STATUS) & 1) == 0)
            __asm__ volatile ("pause");   /* be polite to the (virtual) CPU */

        char c = translate_scancode(inb(PS2_DATA));
        if (c)
            return c;
        /* Unknown scancode: not an error, just a key Phase 1 doesn't map
           (arrows, F-keys…). Silently ignore and keep waiting. */
    }
}
