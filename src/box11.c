#include "box11.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>

#include <xcb/xcb_xrm.h>
#include <cairo/cairo-xcb.h>

struct xcb_context ctx;
struct config config;

static void
config_parse_autosize_args(const char *arg)
{
    for (int i = 0; arg[i] != '\0'; i++) {
        if (arg[i] == 'h') {
            config.autosize_h = true;
        } else if (arg[i] == 'v') {
            config.autosize_v = true;
        } else {
            fprintf(stderr, "Unrecognized autosize direction: \"%c\".\n", arg[i]);
        }
    }
}

void
parse_config(int argc, char *argv[])
{
    /* Initialize default arguments. */
    config.font = "Sans 20";
    config.autosize_h = false;
    config.autosize_v = false;
    config.x = 30;
    config.y = 30;
    config.width = 200;
    config.height = 50;
    config.border = 5;
    config.c_fg = (union rgba)(uint32_t)0xFFFFFFFFU;
    config.c_bg = (union rgba)(uint32_t)0x00000000U;
    config.c_border = (union rgba)(uint32_t)0x88888888U;
    config.padding = 0;
    config.align = PANGO_ALIGN_CENTER;
    config.v_align = CENTER;

    static int flag_help;

    static struct option long_options[] = {
        {"help", no_argument, &flag_help, 1},
        {"font", required_argument, 0, 'f'},
        {"autosize", required_argument, 0, 'u'},
        {"xpos", required_argument, 0, 'x'},
        {"ypos", required_argument, 0, 'y'},
        {"width", required_argument, 0, 'w'},
        {"height", required_argument, 0, 'h'},
        {"border", required_argument, 0, 'b'},
        {"fg-color", required_argument, 0, 't'},
        {"bg-color", required_argument, 0, 'k'},
        {"border-color", required_argument, 0, 0},
        {"padding", required_argument, 0, 'p'},
        {"align", required_argument, 0, 'a'},
        {"vertical-align", required_argument, 0, 'v'},
        {0, 0, 0, 0}
    };

    int opt;
    int long_index;

    inline bool is_longopt(const char *name) {
        return strcmp(long_options[long_index].name, name) == 0;
    }

    while ((opt = getopt_long(argc, argv, "f:u:x:y:w:h:b:t:k:p:a:v:", long_options, &long_index)) != -1) {
        if (flag_help == 1) {
            printf(USAGE);
            exit(EXIT_SUCCESS);
        }

        switch (opt) {
            case 0:
                if (is_longopt("border-color")) {
                    config.c_border = parse_color_string(optarg);
                }
                break;
            case 'f':
                config.font = optarg;
                break;
            case 'u':
                config_parse_autosize_args(optarg);
                break;
            case 'x':
                config.x = atoi(optarg);
                break;
            case 'y':
                config.y = atoi(optarg);
                break;
            case 'w':
                config.width = atoi(optarg);
                break;
            case 'h':
                config.height = atoi(optarg);
                break;
            case 'b':
                config.border = atoi(optarg);
                break;
            case 't':
                config.c_fg = parse_color_string(optarg);
                break;
            case 'k':
                config.c_bg = parse_color_string(optarg);
                break;
            case 'p':
                config.padding = atoi(optarg);
                break;
            case 'a':
                if (*optarg == 'l') {
                    config.align = PANGO_ALIGN_LEFT;
                } else if (*optarg == 'c') {
                    config.align = PANGO_ALIGN_CENTER;
                } else if (*optarg == 'r') {
                    config.align = PANGO_ALIGN_RIGHT;
                } else {
                    fprintf(stderr, "Unrecognized alignment setting.\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case 'v':
                if (*optarg == 't') {
                    config.v_align = TOP;
                } else if (*optarg == 'c') {
                    config.v_align = CENTER;
                } else if (*optarg == 'b') {
                    config.v_align = BOTTOM;
                } else {
                    fprintf(stderr, "Unrecognized alignment setting.\n");
                    exit(EXIT_FAILURE);
                }
                break;
            case ':':
                fprintf(stderr, "Option requires an argument.\n");
                exit(EXIT_FAILURE);
                break;
        }
    }

    /* Provide warnings for ignored parameters. */
    if ((config.autosize_h && config.align != PANGO_ALIGN_CENTER) || (config.autosize_v && config.v_align != CENTER)) {
        fprintf(stderr, "Warning: Alignment settings are ignored when the box is autosized.\n");
    }
}

union rgba
parse_color_string(const char *c)
{
    /* Thanks @lemonboy. */
    union rgba temp = (union rgba)(uint32_t)strtoul(c, NULL, 16);
    return (union rgba){
        .r = (temp.r * temp.a) / 255,
            .g = (temp.g * temp.a) / 255,
            .b = (temp.b * temp.a) / 255,
            .a = temp.a
    };
}

/* Attempts to find an RGBA visual. */
xcb_visualtype_t*
get_visualtype(void)
{
    xcb_depth_iterator_t depth_iter = xcb_screen_allowed_depths_iterator(ctx.screen);
    xcb_visualtype_iterator_t visual_iter;

    if (depth_iter.data) {
        for (; depth_iter.rem; xcb_depth_next(&depth_iter)) {
            if (depth_iter.data->depth == 32) {
                for (visual_iter = xcb_depth_visuals_iterator(depth_iter.data); visual_iter.rem; xcb_visualtype_next(&visual_iter)) {
                    return visual_iter.data;
                }
            }
        }
    }

    return NULL;
}

/* Sets up the primary xcb_context container. */
void
initialize_context(void)
{
    /* XCB */
    ctx.conn = xcb_connect(NULL, NULL);

    ctx.screen = xcb_setup_roots_iterator(xcb_get_setup(ctx.conn)).data;

    ctx.visual = get_visualtype();

    ctx.window = xcb_generate_id(ctx.conn);

    ctx.colormap = xcb_generate_id(ctx.conn);
    xcb_create_colormap(ctx.conn, XCB_COLORMAP_ALLOC_NONE, ctx.colormap, ctx.screen->root, ctx.visual->visual_id);

    xcb_xrm_database_t *xrdb = xcb_xrm_database_from_default(ctx.conn);
    xcb_xrm_resource_get_long(xrdb, "dpi", "box11", &ctx.dpi);
    ctx.scale = ctx.dpi / DPI_SCALE_DIVISOR;

    /* Cairo and Pango */
    ctx.surface = cairo_xcb_surface_create(ctx.conn, ctx.window, ctx.visual, config.width, config.height);
    ctx.cr = cairo_create(ctx.surface);
    ctx.pango = pango_cairo_create_context(ctx.cr);
    pango_cairo_context_set_resolution(ctx.pango, (double) ctx.dpi);
    ctx.layout = pango_layout_new(ctx.pango);
}

void
create_window(void)
{
    initialize_context();

    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP;
    const uint32_t values[5] = {
        config.c_bg.val,
        config.c_border.val,
        1,
        XCB_EVENT_MASK_EXPOSURE,
        ctx.colormap
    };

    int depth = (ctx.visual->visual_id == ctx.screen->root_visual) ? XCB_COPY_FROM_PARENT : 32;
    xcb_create_window(ctx.conn, depth,
            ctx.window, ctx.screen->root,
            config.x, config.y, config.width, config.height, config.border,
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            ctx.visual->visual_id,
            mask, values);

    xcb_change_property(ctx.conn,
            XCB_PROP_MODE_REPLACE,
            ctx.window,
            XCB_ATOM_WM_NAME,
            XCB_ATOM_STRING,
            8,
            strlen("box11"),
            "box11");

    xcb_change_property(ctx.conn,
            XCB_PROP_MODE_REPLACE,
            ctx.window,
            XCB_ATOM_WM_CLASS,
            XCB_ATOM_STRING,
            8,
            strlen("box11"),
            "box11");

    xcb_free_colormap(ctx.conn, ctx.colormap);
}

void
event_loop(void)
{
    char text[1024];
    for (;;) {
        int len = read(0, text, 1024);
        if (text[len - 1] == '\n') {
            text[len - 1] = '\0';
        } else {
            text[len] = '\0';
        }

        if (text[0] == 0) {
            pause();
        }

        draw_text(text);
        xcb_flush(ctx.conn);
    }
}

static
void
autosize_bounds()
{
    PangoRectangle area;
    pango_layout_get_pixel_extents(ctx.layout, NULL, &area);

    cairo_surface_destroy(ctx.surface);

    uint32_t window_dimens[2] = { 0, 0 };

    if (config.autosize_h) {
        const uint32_t value[] = { area.width + config.padding * 2 };
        xcb_configure_window(ctx.conn, ctx.window, XCB_CONFIG_WINDOW_WIDTH, value);

        window_dimens[0] = value[0];

    } else {
        window_dimens[0] = config.width + config.padding * 2;
    }

    if (config.autosize_v) {
        const uint32_t value[] = { area.height + config.padding * 2 };
        xcb_configure_window(ctx.conn, ctx.window, XCB_CONFIG_WINDOW_HEIGHT, value);

        window_dimens[1] = value[0];
    } else {
        window_dimens[1] = config.height + config.padding * 2;
    }

    printf("%dx%d\n", window_dimens[0], window_dimens[1]);

    ctx.surface = cairo_xcb_surface_create(ctx.conn, ctx.window, ctx.visual, window_dimens[0], window_dimens[1]);
    ctx.cr = cairo_create(ctx.surface);

    cairo_translate(ctx.cr, 0, config.padding);
}

void
draw_text(const char *text)
{
    /* Reset the surface. */
    cairo_save(ctx.cr);
    cairo_set_source_rgba(ctx.cr, (double) config.c_bg.r / 255.0, (double) config.c_bg.g / 255.0, (double) config.c_bg.b / 255.0, (double) config.c_bg.a / 255.0);
    cairo_set_operator(ctx.cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(ctx.cr);
    cairo_restore(ctx.cr);

    /* Set the text font. */
    pango_layout_set_markup(ctx.layout, text, -1);
    PangoFontDescription *desc = pango_font_description_from_string(config.font);
    pango_layout_set_font_description(ctx.layout, desc);
    pango_font_description_free(desc);

    if (config.autosize_h || config.autosize_v) {
        autosize_bounds();
    }

    if (!config.autosize_h) {
        pango_layout_set_width(ctx.layout, config.width * PANGO_SCALE);
        pango_layout_set_alignment(ctx.layout, config.align);
    }

    cairo_save(ctx.cr);

    if (!config.autosize_v) {
        pango_layout_set_height(ctx.layout, config.height * PANGO_SCALE);

        int height;
        pango_layout_get_pixel_size(ctx.layout, NULL, &height);

        if (config.v_align == TOP) {
            cairo_translate(ctx.cr, 0, config.padding);
        } else if (config.v_align == CENTER) {
            cairo_translate(ctx.cr, 0, (double) config.height / 2. - (double) height / 2.);
        } else {
            cairo_translate(ctx.cr, 0, (double) config.height - config.padding - height);
        }
    }


    /* Draw text offset by padding. */
    cairo_translate(ctx.cr, config.padding, 0);

    /* Draw the text. */
    cairo_set_source_rgba(ctx.cr, (double) config.c_fg.r / 255.0, (double) config.c_fg.g / 255.0, (double) config.c_fg.b / 255.0, (double) config.c_fg.a / 255.0);
    pango_cairo_show_layout(ctx.cr, ctx.layout);

    cairo_surface_flush(ctx.surface);

    cairo_restore(ctx.cr);
}

int
main(int argc, char *argv[])
{
    parse_config(argc, argv);

    initialize_context();

    create_window();

    xcb_map_window(ctx.conn, ctx.window);

    event_loop();

    xcb_disconnect(ctx.conn);

    return 0;
}
