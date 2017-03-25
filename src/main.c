#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <getopt.h>

#include <xcb/xcb.h>
#include <cairo/cairo-xcb.h>
#include <pango-1.0/pango/pangocairo.h>

#define OVERRIDE_REDIRECT 1

#define CHAR_BUF 4096

#define DEFAULT_WINDOW_X 20
#define DEFAULT_WINDOW_Y 20
#define DEFAULT_WINDOW_WIDTH 300
#define DEFAULT_WINDOW_HEIGHT 40
#define DEFAULT_WINDOW_BORDER 0
#define DEFAULT_FONT "Sans 30"
#define DEFAULT_FG (uint32_t)0xFFFFFFU
#define DEFAULT_BG (uint32_t)0x545454U

#define USAGE "Usage: xbox [OPTIONS]\n"\
"   --help              Show this help\n"\
"-f --font [FONT]       Print text with font FONT\n"\
"-x --xpos [X]          Set the x-coordinate of the box\n"\
"-y --ypos [Y]          Set the y-coordinate of the box\n"\
"-w --width [WIDTH]     Set the width of the box\n"\
"-h --height [HEIGHT]   Set the height of the box\n"\
"-b --border [BORDER]   Set the border width of the box\n"\
"-t --fg-color [COLOR]  Draw text with the color COLOR\n"\
"-k --bg-color [COLOR]  Draw box with the background color COLOR\n"\
"-a --align (l|c|r)     Horizontally align text left, center, or right\n"\
"-p --padding [PADDING] Horizontally pad the text by PADDING pixels\n"\
"\nColors should be given in the form RRGGBB.\n"\
"All measurements are in pixels.\n"

union rgb {
        struct {
                uint8_t b;
                uint8_t g;
                uint8_t r;
        };
        uint32_t v;
};

struct opts {
        const char *font;
        unsigned int x, y, width, height;
        unsigned int border;
        PangoAlignment align;
        int padding;
        union rgb fg, bg;
};

/* No more global pointers in L.A., please baby no more global pointers in L.A. */
static xcb_connection_t *conn;
static xcb_screen_t *screen;
static xcb_window_t window;
static xcb_visualtype_t *visual;
static cairo_surface_t *surface;
static cairo_t *cr;
static PangoLayout *layout;

static struct opts *o;

static
void
usage()
{
        printf(USAGE);
}

static
union rgb
parse_color_string(const char *color)
{
        return (union rgb)(uint32_t)strtoul(color, NULL, 16);
}

static
void
parse_opts(int argc, char *argv[])
{
        o = (struct opts*) malloc(sizeof(struct opts));

        /* Initialize default arguments. */
        o->font = DEFAULT_FONT;
        o->x = DEFAULT_WINDOW_X;
        o->y = DEFAULT_WINDOW_Y;
        o->width = DEFAULT_WINDOW_WIDTH;
        o->height = DEFAULT_WINDOW_HEIGHT;
        o->border = DEFAULT_WINDOW_BORDER;
        o->fg = (union rgb)DEFAULT_FG;
        o->bg = (union rgb)DEFAULT_BG;
        o->align = PANGO_ALIGN_CENTER;
        o->padding = 0;

        static int help_flag;

        static struct option long_options[] = {
                {"help", no_argument, &help_flag, 1},
                {"font", required_argument, 0, 'f'},
                {"x", required_argument, 0, 'x'},
                {"y", required_argument, 0, 'y'},
                {"width", required_argument, 0, 'w'},
                {"height", required_argument, 0, 'h'},
                {"border", required_argument, 0, 'b'},
                {"fg-color", required_argument, 0, 't'},
                {"bg-color", required_argument, 0, 'k'},
                {"align", required_argument, 0, 'a'},
                {"padding", required_argument, 0, 'p'},
                {0, 0, 0, 0}
        };

        /* Set to given arguments or print information. */
        int opt;
        int long_index;
        while ((opt = getopt_long(argc, argv, ":f:x:y:w:h:b:t:k:a:p:", long_options, &long_index)) != -1) {
                if (help_flag == 1) {
                        usage();
                        exit(EXIT_SUCCESS);
                }

                switch (opt) {
                        case 'f':
                                o->font = optarg;
                                break;
                        case 'x':
                                o->x = atoi(optarg);
                                break;
                        case 'y':
                                o->y = atoi(optarg);
                                break;
                        case 'w':
                                o->width = atoi(optarg);
                                break;
                        case 'h':
                                o->height = atoi(optarg);
                                break;
                        case 'b':
                                o->border = atoi(optarg);
                                break;
                        case 't':
                                o->fg = parse_color_string(optarg);
                                break;
                        case 'k':
                                o->bg = parse_color_string(optarg);
                                break;
                        case 'a':
                                if (*optarg == 'l') {
                                        o->align = PANGO_ALIGN_LEFT;
                                } else if (*optarg == 'c') {
                                        o->align = PANGO_ALIGN_CENTER;
                                } else if (*optarg == 'r') {
                                        o->align = PANGO_ALIGN_RIGHT;
                                } else {
                                        printf("Unrecognized alignment setting.\n");
                                        exit(EXIT_FAILURE);
                                }
                                break;
                        case 'p':
                                o->padding = atoi(optarg);
                                break;
                        case ':':
                                printf("Option requires an argument.\n");
                        default:
                                usage();
                                exit(EXIT_FAILURE);
                }
        }
}

static
xcb_visualtype_t*
find_visualtype()
{
        xcb_depth_iterator_t depth_iter = xcb_screen_allowed_depths_iterator(screen);

        for (; depth_iter.rem; xcb_depth_next(&depth_iter)) {
                xcb_visualtype_iterator_t visualtype_iter = xcb_depth_visuals_iterator(depth_iter.data);
                for (; visualtype_iter.rem; xcb_visualtype_next(&visualtype_iter)) {
                        if (screen->root_visual == visualtype_iter.data->visual_id) {
                                return visualtype_iter.data;
                        }
                }
        }

        return NULL;
}

static
void
draw_text(const char *text)
{
        /* Initialize the Pango layout. */
        pango_layout_set_width(layout, o->width * PANGO_SCALE);
        pango_layout_set_height(layout, o->height * PANGO_SCALE);
        pango_layout_set_alignment(layout, o->align);

        /* Account for padding. */
        pango_layout_set_width(layout, pango_layout_get_width(layout) - o->padding * 2 * PANGO_SCALE);

        /* Set the text font. */
        pango_layout_set_text(layout, text, -1);
        PangoFontDescription *desc = pango_font_description_from_string(o->font);
        pango_layout_set_font_description(layout, desc);
        pango_font_description_free(desc);

        /* Vertically center the text. */
        int height;
        pango_layout_get_pixel_size(layout, NULL, &height);
        cairo_translate(cr, o->padding, (double) o->height / 2. - (double) height / 2.);

        /* Draw the text. */
        cairo_set_source_rgb(cr, (double) o->fg.r / 255.0, (double) o->fg.g / 255.0, (double) o->fg.b / 255.0);
        pango_cairo_show_layout(cr, layout);

        cairo_surface_flush(surface);
}

int main(int argc, char *argv[])
{
        parse_opts(argc, argv);

        /* First, load input. */
        char text[CHAR_BUF];
        int length = read(0, text, sizeof(text));

        /* Ignore trailing newlines. */
        if (length > 0 && text[length - 1] == '\n') {
                text[length - 1] = '\0';
        } else {
                text[length] = '\0';
        }

        /* Initialize X connection so we can do stuff with X. */
        conn = xcb_connect(NULL, NULL);
        if (!conn) {
                printf("Unable to connect to the X server!\n");
                return -1;
        }

        /* Setup the screen and window. */
        window = xcb_generate_id(conn);
        screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;

        visual = find_visualtype();
        if (visual == NULL) {
                printf("Error getting a visualtype.");
                xcb_disconnect(conn);
                return -1;
        }

        unsigned int mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
        unsigned int values[2];
        values[0] = o->bg.v;
        values[1] = XCB_EVENT_MASK_EXPOSURE;

        xcb_create_window(conn,
                        screen->root_depth,
                        window,
                        screen->root,
                        o->x, o->y,
                        o->width, o->height,
                        o->border,
                        XCB_WINDOW_CLASS_INPUT_OUTPUT,
                        screen->root_visual,
                        mask, values);

        xcb_change_property(conn,
                        XCB_PROP_MODE_REPLACE,
                        window,
                        XCB_ATOM_WM_NAME,
                        XCB_ATOM_STRING,
                        8,
                        strlen("xbox"),
                        "xbox");

        xcb_change_property(conn,
                        XCB_PROP_MODE_REPLACE,
                        window,
                        XCB_ATOM_WM_CLASS,
                        XCB_ATOM_STRING,
                        8,
                        strlen("xbox"),
                        "xbox");

        /* Setup floating. */
        unsigned int override[1] = { OVERRIDE_REDIRECT };
        xcb_change_window_attributes(conn,
                        window,
                        XCB_CW_OVERRIDE_REDIRECT,
                        override);

        /* Engage drawing. */
        xcb_map_window(conn, window);

        /* Initialize the graphical objects. */
        surface = cairo_xcb_surface_create(conn, window, visual, o->width, o->height);
        cr = cairo_create(surface);
        layout = pango_cairo_create_layout(cr);

        xcb_flush(conn);

        /* Begin the event loop. */
        xcb_generic_event_t *event;
        while((event = xcb_wait_for_event(conn))) {
                switch (event->response_type & ~0x80) {
                        case XCB_EXPOSE:
                                draw_text(text);
                                xcb_flush(conn);
                                break;
                        default:
                                printf("Unrecognized event -- this shouldn't happen!");
                                break;
                }

                free(event);
        }

        /*
         * We're done with our resources!
         * Well, actually, this code path will never be reached, since the program can only end by SIGKILL.
         * Maybe in another life.
         */
        cairo_surface_finish(surface);
        xcb_disconnect(conn);

        return 0;
}
