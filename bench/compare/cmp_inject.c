/* Real input events for the comparison programs, made by the XTEST extension on a second connection to the X server. Kept in a
   C file of its own: <X11/Xlib.h> defines macros (None, Bool, Status...) that the iGUI headers use as names. */
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#include <unistd.h>

static Display* g_d;

int cmp_inj_open(void)
{
    int a, b, c, e;
    g_d = XOpenDisplay(0);
    return g_d && XTestQueryExtension(g_d, &a, &b, &c, &e);
}
void cmp_inj_flush(void) /* the server has processed everything and written the events to the application */
{
    XSync(g_d, False);
    usleep(400);
}
void cmp_inj_xflush(void) { XFlush(g_d); } /* send, do not wait */
void cmp_inj_move(int x, int y) { XTestFakeMotionEvent(g_d, DefaultScreen(g_d), x, y, 0); }
void cmp_inj_button(int b, int down) { XTestFakeButtonEvent(g_d, (unsigned)b, down ? True : False, 0); }
void cmp_inj_key(unsigned long keysym)
{
    KeyCode kc = XKeysymToKeycode(g_d, (KeySym)keysym);
    XTestFakeKeyEvent(g_d, kc, True, 0);
    XTestFakeKeyEvent(g_d, kc, False, 0);
}
