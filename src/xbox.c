#include "xbox.h"

struct xcb_context ctx;
struct config config;

void
parse_config(int argc, char *argv[])
{
        /* Initialize default arguments. */
        config.font = "Sans 30";
        config.autosize = false;
        config.return_autosize = false;
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
        static int flag_c_border;
        static int flag_return_autosize;

        static struct option long_options[] = {
                {"help", no_argument, &flag_help, 1},
                {"font", required_argument, 0, 'f'},
                {"autosize", no_argument, 0, 'u'},
                {"return-autosize", no_argument, &flag_return_autosize, 1},
                {"xpos", required_argument, 0, 'x'},
                {"ypos", required_argument, 0, 'y'},
                {"width", required_argument, 0, 'w'},
                {"height", required_argument, 0, 'h'},
                {"border", required_argument, 0, 'b'},
                {"fg-color", required_argument, 0, 't'},
                {"bg-color", required_argument, 0, 'k'},
                {"border-color", required_argument, &flag_c_border, 1},
                {"padding", required_argument, 0, 'p'},
                {"align", required_argument, 0, 'a'},
                {"vertical-align", required_argument, 0, 'v'},
                {0, 0, 0, 0}
        };

        /* Set to given arguments or print information. */
        int opt;
        int long_index;
        while ((opt = getopt_long(argc, argv, "f:ux:y:w:h:b:t:k:p:a:v:", long_options, &long_index)) != -1) {
                if (flag_help == 1) {
                        printf(USAGE);
                        exit(EXIT_SUCCESS);
                }

                if (flag_c_border == 1) {
                        printf("Setting border color.\n");
                        config.c_border = parse_color_string(optarg);
                }

                if (flag_return_autosize == 1) {
                        config.return_autosize = true;
                }

                switch (opt) {
                        case 'f':
                                config.font = optarg;
                                break;
                        case 'u':
                                config.autosize = true;
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
                                        printf("Unrecognized alignment setting.\n");
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
                                        printf("Unrecognized alignment setting.\n");
                                        exit(EXIT_FAILURE);
                                }
                                break;
                        case ':':
                                printf("Option requires an argument.\n");
                                exit(EXIT_FAILURE);
                }
        }

        /* Provide warnings for ignored parameters. */
        if (config.autosize && (config.v_align != CENTER || config.align != PANGO_ALIGN_CENTER)) {
                fprintf(stderr, "Warning: Alignment settings are ignored when the box is autosized.");
        }

        if (config.return_autosize && !config.autosize) {
                fprintf(stderr, "Error: Cannot print autosize dimensions if autosize is not enabled!\n");
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
        xcb_xrm_resource_get_long(xrdb, "dpi", "xbox", &ctx.dpi);
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
        printf("Using a %d-bit depth for drawing.\n", depth);
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
                        strlen("xbox"),
                        "xbox");

        xcb_change_property(ctx.conn,
                        XCB_PROP_MODE_REPLACE,
                        ctx.window,
                        XCB_ATOM_WM_CLASS,
                        XCB_ATOM_STRING,
                        8,
                        strlen("xbox"),
                        "xbox");

        xcb_free_colormap(ctx.conn, ctx.colormap);
}

void
event_loop(void)
{
        xcb_generic_event_t *event;
        while ((event = xcb_wait_for_event(ctx.conn)) != NULL) {
                free(event);
                xcb_flush(ctx.conn);
        }
}

void
draw_text(const char *text)
{
        /* Set the text font. */
        pango_layout_set_text(ctx.layout, text, -1);
        PangoFontDescription *desc = pango_font_description_from_string(config.font);
        pango_layout_set_font_description(ctx.layout, desc);
        pango_font_description_free(desc);

        /* TODO: There's some redundancy in this section. */
        if (config.autosize) {
                PangoRectangle area;
                pango_layout_get_pixel_extents(ctx.layout, NULL, &area);
                pango_layout_set_width(ctx.layout, area.width);
                pango_layout_set_height(ctx.layout, area.height * g_slist_length(pango_layout_get_lines_readonly(ctx.layout)));
                cairo_surface_destroy(ctx.surface);
                ctx.surface = cairo_xcb_surface_create(ctx.conn, ctx.window, ctx.visual, area.width + config.padding * 2, area.height + config.padding * 2);
                ctx.cr = cairo_create(ctx.surface);

                const uint32_t values[] = { area.width + config.padding * 2, area.height + config.padding * 2 };
                xcb_configure_window(ctx.conn, ctx.window, XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);

                cairo_translate(ctx.cr, 0, config.padding);

                if (config.return_autosize) {
                        printf("AUTOSIZE:%dx%d\n", values[0], values[1]);
                }
        } else {
                /* Initialize the Pango layout. */
                pango_layout_set_width(ctx.layout, config.width * PANGO_SCALE);
                pango_layout_set_height(ctx.layout, config.height * PANGO_SCALE);

                /* Account for padding. */
                pango_layout_set_width(ctx.layout, pango_layout_get_width(ctx.layout) - config.padding * 2 * PANGO_SCALE);
                pango_layout_set_alignment(ctx.layout, config.align);

                /* Vertically center the text. */
                int height;
                pango_layout_get_pixel_size(ctx.layout, NULL, &height);

                cairo_translate(ctx.cr, config.padding, 0);
                if (config.v_align == TOP) {
                        cairo_translate(ctx.cr, 0, config.padding);
                } else if (config.v_align == CENTER) {
                        cairo_translate(ctx.cr, 0, (double) config.height / 2. - (double) height / 2.);
                } else {
                        cairo_translate(ctx.cr, 0, (double) config.height - config.padding - height);
                }
        }

        cairo_translate(ctx.cr, config.padding, 0);

        /* Draw the text. */
        cairo_set_source_rgba(ctx.cr, (double) config.c_fg.r / 255.0, (double) config.c_fg.g / 255.0, (double) config.c_fg.b / 255.0, (double) config.c_fg.a / 255.0);
        pango_cairo_show_layout(ctx.cr, ctx.layout);

        cairo_surface_flush(ctx.surface);
}

int
main(int argc, char *argv[])
{
        parse_config(argc, argv);

        char text[1024];
        int len = read(0, text, 1024);
        if (text[len - 1] == '\n') {
                text[len - 1] = '\0';
        } else {
                text[len] = '\0';
        }

        initialize_context();

        create_window();

        xcb_map_window(ctx.conn, ctx.window);
        draw_text(text);
        xcb_flush(ctx.conn);

        event_loop();
        xcb_disconnect(ctx.conn);
        return 0;
}
