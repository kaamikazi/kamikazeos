/* keyboard.c — polling PS/2 keyboard driver. See keyboard.h. */
#include "keyboard.h"
#include "errors.h"
#include "io.h"

#define PS2_DATA    0x60   /* read scancodes here                    */
#define PS2_STATUS  0x64   /* bit 0 set = a byte is waiting in DATA  */

static int present;        /* did the controller answer at boot?     */
static int shift_down;     /* live state of the Shift keys           */

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

void keyboard_init(void)
{
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
              "PS/2 keyboard attached (polling mode)", "no action needed");
}

int keyboard_present(void)
{
    return present;
}

char keyboard_getchar(void)
{
    for (;;) {
        /* Wait until the controller has a byte for us (status bit 0). */
        while ((inb(PS2_STATUS) & 1) == 0)
            __asm__ volatile ("pause");   /* be polite to the (virtual) CPU */

        uint8_t sc = inb(PS2_DATA);

        /* Track Shift so letters/symbols come out right. */
        if (sc == SC_LSHIFT_DOWN || sc == SC_RSHIFT_DOWN) {
            shift_down = 1;
            continue;
        }
        if (sc == (SC_LSHIFT_DOWN | SC_RELEASED) ||
            sc == (SC_RSHIFT_DOWN | SC_RELEASED)) {
            shift_down = 0;
            continue;
        }

        /* Ignore all other key-RELEASE events; we act on presses. */
        if (sc & SC_RELEASED)
            continue;

        /* Translate the press to ASCII if it's a key we know. */
        if (sc < sizeof(map_lower)) {
            char c = shift_down ? map_upper[sc] : map_lower[sc];
            if (c != 0)
                return c;
        }
        /* Unknown scancode: not an error, just a key Phase 1 doesn't map
           (arrows, F-keys…). Silently ignore and keep waiting. */
    }
}
