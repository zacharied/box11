#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <xcb/xcb.h>
#include <cairo/cairo-xcb.h>
#include <pango-1.0/pango/pangocairo.h>

#define CHAR_BUF 4096

#define WINDOW_X 20
#define WINDOW_Y 20
#define WINDOW_WIDTH 300
#define WINDOW_HEIGHT 40
#define WINDOW_BORDER 0
#define FONT "Sans 30"

static xcb_connection_t *conn;
static xcb_screen_t *screen;
static xcb_window_t window;
static xcb_visualtype_t *visual;
static cairo_surface_t *surface;
static cairo_t *cr;
static PangoLayout *layout;

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
        pango_layout_set_width(layout, WINDOW_WIDTH * PANGO_SCALE);
        pango_layout_set_height(layout, WINDOW_HEIGHT * PANGO_SCALE);
        pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);

        /* Set the text font. */
        pango_layout_set_text(layout, text, -1);
        PangoFontDescription *desc = pango_font_description_from_string(FONT);
        pango_layout_set_font_description(layout, desc);
        pango_font_description_free(desc);

        /* Vertically center the text. */
        PangoRectangle rect;
        pango_layout_get_pixel_extents(layout, NULL, &rect);
        cairo_translate(cr, 0, WINDOW_HEIGHT / 2.0 - rect.height / 2.0 - rect.y);

        /* Draw the text. */
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        pango_cairo_show_layout(cr, layout);

        cairo_surface_flush(surface);
}

int main(int argc, char *argv[])
{
        /* First, load input. */
        char text[CHAR_BUF];
        int length = read(0, text, sizeof(text));
        text[length] = '\0';

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
        values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS;

        xcb_create_window(conn,
                screen->root_depth,
                window,
                screen->root,
                WINDOW_X, WINDOW_Y,
                WINDOW_WIDTH, WINDOW_HEIGHT,
                WINDOW_BORDER,
                XCB_WINDOW_CLASS_INPUT_OUTPUT,
                screen->root_visual,
                mask, values);

        /* Setup floating. */
        unsigned int override[1] = { 1 };
        xcb_change_window_attributes(conn,
                        window,
                        XCB_CW_OVERRIDE_REDIRECT,
                        override);

        /* Engage drawing. */
        xcb_map_window(conn, window);

        /* Initialize the graphical objects. */
        surface = cairo_xcb_surface_create(conn, window, visual, WINDOW_WIDTH, WINDOW_HEIGHT);
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
                }


                free(event);
        }


        /* We're done with our resources! */
        cairo_surface_finish(surface);
        xcb_disconnect(conn);

        return 0;
}
