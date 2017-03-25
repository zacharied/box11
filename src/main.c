#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

struct opts {
        const char *font;
        unsigned int x, y, width, height;
        unsigned int border;
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
const char*
usage() {
        return "Usage: xbox [-u] [-f font] [-x xpos] [-y ypos] [-w width] [-h height] [-b border]\n";
}

static
void
parse_opts(int argc, char *argv[]) {
        o = (struct opts*) malloc(sizeof(struct opts));

        /* Initialize default arguments. */
        o->font = DEFAULT_FONT;
        o->x = DEFAULT_WINDOW_X;
        o->y = DEFAULT_WINDOW_Y;
        o->width = DEFAULT_WINDOW_WIDTH;
        o->height = DEFAULT_WINDOW_HEIGHT;
        o->border = DEFAULT_WINDOW_BORDER;

        /* Set to given arguments or print information. */
        int opt;
        while ((opt = getopt(argc, argv, "uf:x:y:w:h:b:")) != -1) {
                switch (opt) {
                        case 'u':
                                printf(usage());
                                exit(EXIT_SUCCESS);
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
                        default:
                                printf(usage());
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
        pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);

        /* Set the text font. */
        pango_layout_set_text(layout, text, -1);
        PangoFontDescription *desc = pango_font_description_from_string(o->font);
        pango_layout_set_font_description(layout, desc);
        pango_font_description_free(desc);

        /* Vertically center the text. */
        int height;
        pango_layout_get_pixel_size(layout, NULL, &height);
        cairo_translate(cr, 0, (double) o->height / 2. - (double) height / 2.);

        /* Draw the text. */
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
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
        values[0] = screen->white_pixel;
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
                                printf("Received expose event.\n");
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
