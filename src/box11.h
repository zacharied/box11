#include <stdbool.h>
#include <stdint.h>
#include <xcb/xcb.h>
#include <pango-1.0/pango/pangocairo.h>

#define APP_NAME "box11"

#define USAGE \
    "Usage: box11 [OPTIONS]\n"\
    "   --help              Show this help\n"\
    "-z --bounds            Print the size of the box that would be drawn and then exit\n"\
    "-f --font [FONT]       Print text with font FONT\n"\
    "-u --autosize (h)(v)   Adjust the window dimensions to fit the text. Overrides other positioning arguments\n"\
    "-x --xpos [X]          Set the x-coordinate of the box\n"\
    "-y --ypos [Y]          Set the y-coordinate of the box\n"\
    "-w --width [WIDTH]     Set the width of the box\n"\
    "-h --height [HEIGHT]   Set the height of the box\n"\
    "-b --border [BORDER]   Set the border width of the box\n"\
    "-t --fg-color [COLOR]  Draw text with the color COLOR\n"\
    "-k --bg-color [COLOR]  Draw box with the background color COLOR\n"\
    "-o --border-color [COLOR]\n"\
    "                       Draw box border with color COLOR\n"\
    "-a --align (l|c|r)     Horizontally align text left, center, or right\n"\
    "-p --padding [PADDING] Horizontally pad the text by PADDING pixels\n"\
    "-v --vertical-align (t|c|b)\n"\
    "                       Vertically align text top, center, or bottom\n"\
    "\nColors should be given in the form AARRGGBB.\n"\
    "All measurements are in pixels.\n"

#define DPI_SCALE_DIVISOR 96

/* An RGBA color map represented as a 32-bit integer. */
union rgba {
    struct {
        uint8_t b;
        uint8_t g;
        uint8_t r;
        uint8_t a;
    };
    uint32_t val;
};

typedef enum vertical_align {
    TOP,
    CENTER,
    BOTTOM
} vertical_align;

/* Data necessary for interacting with the X server. */
struct xcb_context {
    xcb_connection_t *conn;
    xcb_screen_t *screen;
    xcb_visualtype_t *visual;
    xcb_window_t window;
    xcb_colormap_t colormap;

    cairo_t *cr;
    cairo_surface_t *surface;
    PangoContext *pango;
    PangoLayout *layout;

    long dpi;
    uint32_t scale;
};

/* Given command-line arguments. */
struct config {
    const char *font;
    bool autosize_h, autosize_v;
    bool bounds_only;
    uint32_t x, y, width, height;
    uint32_t border;
    uint32_t padding;
    union rgba c_fg, c_bg, c_border;
    PangoAlignment align;
    vertical_align v_align;
};

void
parse_config(int argc, char *argv[]);

union rgba
parse_color_string(const char *c);

xcb_visualtype_t*
get_visualtype(void);

void
initialize_context(void);

void
create_window(void);

void
event_loop(void);

void
draw_text(const char *text);
