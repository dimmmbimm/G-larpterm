#include "config.h"

#include <gtk/gtk.h>
#include <vte/vte.h>
#include <glib.h>
#include <fontconfig/fontconfig.h>
#include <cairo.h>

#include <pwd.h>
#include <unistd.h>
#include <locale.h>
#include <stdio.h>

#define APP_NAME "G-larpterm"
#define APP_ID "g-larpterm"
#define APP_VERSION "1.0.0"
#define TITLEBAR_HEIGHT 24
#define WINDOW_BORDER 2
#define RESIZE_EDGE 4
#define EDGE_WEST 1
#define EDGE_EAST 2
#define EDGE_NORTH 4
#define EDGE_SOUTH 8

typedef struct SpawnData SpawnData;

typedef struct {
    GlarpConfig config;
    GtkWidget *window;
    GtkWidget *terminal;
    GtkWidget *scrollbar;
    GtkWidget *titlebar;
    GtkWidget *title_label;
    GtkWidget *menu;
    GtkWidget *menu_copy;
    double font_scale;
    int resize_edge;
    SpawnData *spawn;
} App;

struct SpawnData {
    App *app;
    char **environment;
    char *directory;
};

static void create_app(const GlarpConfig *config, const char *const *command);
static void app_copy(App *app);
static void app_paste(App *app);
static void app_paste_primary(App *app);
static void app_select_all(App *app);
static void app_close(App *app);
static void app_new_window(App *app);
static void app_zoom(App *app, double change);
static void app_zoom_reset(App *app);
static void app_scroll(App *app, double amount);
static void app_fullscreen(App *app);

static int window_count;
static gboolean pixel_font_ready;

static const char theme_css[] =
    "window { background-color: #000080; }\n"
    ".glarp-window-frame { background-color: #ffffff; border: 2px solid #000080; }\n"
    ".glarp-titlebar { background-color: #000080; padding: 0; min-height: 24px; }\n"
    ".glarp-title-label { color: #ffffff; padding: 0; }\n"
    ".glarp-title-button { background-color: #000080; color: #ffffff; border: 1px solid #ffffff; border-radius: 0; padding: 0; min-width: 18px; min-height: 18px; font-family: \"G-Larpterm Pixel\"; font-size: 12pt; }\n"
    ".glarp-title-button label { padding: 0; }\n"
    ".glarp-title-button:hover { background-color: #ffffff; color: #000080; }\n"
    ".glarp-title-button:active { background-color: #008080; color: #ffffff; }\n"
    ".glarp-terminal-frame { background-color: #ffffff; }\n"
    "scrollbar { background-color: #ffffff; border-left: 1px solid #000080; }\n"
    "scrollbar trough { background-color: #ffffff; border: none; }\n"
    "scrollbar slider { background-color: #000080; border: 1px solid #ffffff; border-radius: 0; min-width: 8px; min-height: 32px; }\n"
    "scrollbar slider:hover { background-color: #008080; }\n"
    "scrollbar button { min-width: 0; min-height: 0; padding: 0; border: none; background-color: transparent; }\n"
    "menu { background-color: #ffffff; border: 2px solid #000080; border-radius: 0; padding: 0; }\n"
    "menuitem { color: #000080; padding: 3px 12px; font-family: \"G-Larpterm Pixel\"; font-size: 12pt; }\n"
    "menuitem:hover { background-color: #000080; color: #ffffff; }\n"
    "menu separator { background-color: #000080; min-height: 2px; }\n";

static char *find_data_file(const char *name)
{
    char *executable = g_file_read_link("/proc/self/exe", NULL);
    char *directory;
    char *candidate;

    if (executable == NULL) {
        return NULL;
    }

    directory = g_path_get_dirname(executable);
    g_free(executable);

    candidate = g_build_filename(directory, "..", "data", name, NULL);
    if (g_file_test(candidate, G_FILE_TEST_IS_REGULAR)) {
        g_free(directory);
        return candidate;
    }
    g_free(candidate);

    candidate = g_build_filename(directory, "..", "share", "g-larpterm", name, NULL);
    g_free(directory);

    if (g_file_test(candidate, G_FILE_TEST_IS_REGULAR)) {
        return candidate;
    }

    g_free(candidate);
    return NULL;
}

static gboolean load_pixel_font(void)
{
    char *path;
    gboolean loaded = FALSE;

    FcInit();
    path = find_data_file("g-larpterm-pixel.ttf");

    if (path != NULL) {
        loaded = FcConfigAppFontAddFile(NULL, (const FcChar8 *)path) != 0;
        g_free(path);
    }

    return loaded;
}

static void install_theme(void)
{
    GtkCssProvider *provider = gtk_css_provider_new();
    cairo_font_options_t *options = cairo_font_options_create();

    cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_NONE);
    cairo_font_options_set_hint_style(options, CAIRO_HINT_STYLE_NONE);
    cairo_font_options_set_hint_metrics(options, CAIRO_HINT_METRICS_OFF);
    gdk_screen_set_font_options(gdk_screen_get_default(), options);
    cairo_font_options_destroy(options);

    gtk_css_provider_load_from_data(provider, theme_css, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

static void apply_colors(VteTerminal *terminal)
{
    static const char *const color_names[16] = {
        "#000000", "#800000", "#008000", "#808000",
        "#000080", "#800080", "#008080", "#000080",
        "#404040", "#c00000", "#00a000", "#a08000",
        "#0000ff", "#a000a0", "#006060", "#000000"
    };
    GdkRGBA foreground;
    GdkRGBA background;
    GdkRGBA cursor;
    GdkRGBA highlight;
    GdkRGBA highlight_foreground;
    GdkRGBA palette[16];
    int index;

    gdk_rgba_parse(&foreground, "#000080");
    gdk_rgba_parse(&background, "#ffffff");
    gdk_rgba_parse(&cursor, "#000080");
    gdk_rgba_parse(&highlight, "#008000");
    gdk_rgba_parse(&highlight_foreground, "#ffffff");

    for (index = 0; index < 16; index++) {
        gdk_rgba_parse(&palette[index], color_names[index]);
    }

    vte_terminal_set_colors(terminal, &foreground, &background, palette, 16);
    vte_terminal_set_color_cursor(terminal, &cursor);
    vte_terminal_set_color_cursor_foreground(terminal, &highlight_foreground);
    vte_terminal_set_color_highlight(terminal, &highlight);
    vte_terminal_set_color_highlight_foreground(terminal, &highlight_foreground);
}

static void set_window_title(App *app, const char *title)
{
    const char *text = title != NULL && *title != '\0' ? title : APP_NAME;

    if (app->window != NULL) {
        gtk_window_set_title(GTK_WINDOW(app->window), text);
    }

    if (app->title_label != NULL) {
        gtk_label_set_text(GTK_LABEL(app->title_label), text);
    }
}

static void apply_font(App *app)
{
    const char *name = app->config.font;
    PangoFontDescription *font;
    cairo_font_options_t *options;
    int size;

    if (!pixel_font_ready && g_strcmp0(name, GLARPTERM_DEFAULT_FONT) == 0) {
        name = "DejaVu Sans Mono 12";
    }

    font = pango_font_description_from_string(name);
    size = pango_font_description_get_size(font) / PANGO_SCALE;

    if (size > 0) {
        pango_font_description_set_absolute_size(font, size * PANGO_SCALE);
    }

    vte_terminal_set_font(VTE_TERMINAL(app->terminal), font);
    pango_font_description_free(font);

    options = cairo_font_options_create();
    cairo_font_options_set_antialias(options, CAIRO_ANTIALIAS_NONE);
    cairo_font_options_set_hint_style(options, CAIRO_HINT_STYLE_NONE);
    cairo_font_options_set_hint_metrics(options, CAIRO_HINT_METRICS_OFF);
    vte_terminal_set_font_options(VTE_TERMINAL(app->terminal), options);
    cairo_font_options_destroy(options);
}

static void style_title_label(GtkWidget *label)
{
    PangoAttrList *attributes = pango_attr_list_new();

    pango_attr_list_insert(
        attributes,
        pango_attr_family_new(pixel_font_ready ? "G-Larpterm Pixel" : "DejaVu Sans Mono"));
    pango_attr_list_insert(attributes, pango_attr_size_new_absolute(16 * PANGO_SCALE));
    gtk_label_set_attributes(GTK_LABEL(label), attributes);
    pango_attr_list_unref(attributes);
}

static int edge_from_position(double x, double y, int width, int height)
{
    int edge = 0;

    if (x < RESIZE_EDGE) {
        edge |= EDGE_WEST;
    } else if (x >= width - RESIZE_EDGE) {
        edge |= EDGE_EAST;
    }

    if (y < RESIZE_EDGE) {
        edge |= EDGE_NORTH;
    } else if (y >= height - RESIZE_EDGE) {
        edge |= EDGE_SOUTH;
    }

    return edge;
}

static const char *resize_cursor_name(int edge)
{
    switch (edge) {
    case EDGE_WEST | EDGE_NORTH:
        return "nw-resize";
    case EDGE_NORTH:
        return "n-resize";
    case EDGE_EAST | EDGE_NORTH:
        return "ne-resize";
    case EDGE_WEST:
        return "w-resize";
    case EDGE_EAST:
        return "e-resize";
    case EDGE_WEST | EDGE_SOUTH:
        return "sw-resize";
    case EDGE_SOUTH:
        return "s-resize";
    case EDGE_EAST | EDGE_SOUTH:
        return "se-resize";
    default:
        return NULL;
    }
}

static GdkWindowEdge window_edge(int edge)
{
    switch (edge) {
    case EDGE_WEST | EDGE_NORTH:
        return GDK_WINDOW_EDGE_NORTH_WEST;
    case EDGE_NORTH:
        return GDK_WINDOW_EDGE_NORTH;
    case EDGE_EAST | EDGE_NORTH:
        return GDK_WINDOW_EDGE_NORTH_EAST;
    case EDGE_WEST:
        return GDK_WINDOW_EDGE_WEST;
    case EDGE_EAST:
        return GDK_WINDOW_EDGE_EAST;
    case EDGE_WEST | EDGE_SOUTH:
        return GDK_WINDOW_EDGE_SOUTH_WEST;
    case EDGE_SOUTH:
        return GDK_WINDOW_EDGE_SOUTH;
    default:
        return GDK_WINDOW_EDGE_SOUTH_EAST;
    }
}

static void update_resize_cursor(App *app, int edge)
{
    GdkDisplay *display;
    GdkWindow *window;
    GdkWindow *terminal_window;
    GdkWindow *scrollbar_window;
    GdkCursor *cursor = NULL;
    GdkCursor *text_cursor = NULL;
    const char *name;

    if (app->resize_edge == edge) {
        return;
    }

    app->resize_edge = edge;
    window = gtk_widget_get_window(app->window);

    if (window == NULL) {
        return;
    }

    display = gdk_window_get_display(window);
    terminal_window = app->terminal != NULL ? gtk_widget_get_window(app->terminal) : NULL;
    scrollbar_window = app->scrollbar != NULL ? gtk_widget_get_window(app->scrollbar) : NULL;

    if (edge != 0) {
        name = resize_cursor_name(edge);
        cursor = gdk_cursor_new_from_name(display, name);
    } else {
        text_cursor = gdk_cursor_new_from_name(display, "text");
    }

    gdk_window_set_cursor(window, cursor);

    if (terminal_window != NULL) {
        gdk_window_set_cursor(terminal_window, edge != 0 ? cursor : text_cursor);
    }

    if (scrollbar_window != NULL) {
        gdk_window_set_cursor(scrollbar_window, cursor);
    }

    if (cursor != NULL) {
        g_object_unref(cursor);
    }

    if (text_cursor != NULL) {
        g_object_unref(text_cursor);
    }
}

static gboolean on_window_button(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    App *app = user_data;
    int width = gtk_widget_get_allocated_width(app->window);
    int height = gtk_widget_get_allocated_height(app->window);
    int edge = edge_from_position(event->x, event->y, width, height);
    gboolean in_titlebar = event->x >= WINDOW_BORDER &&
                           event->x < width - WINDOW_BORDER &&
                           event->y >= WINDOW_BORDER &&
                           event->y < WINDOW_BORDER + TITLEBAR_HEIGHT;

    (void)widget;

    if (event->button != 1) {
        return FALSE;
    }

    if (event->type == GDK_2BUTTON_PRESS && in_titlebar) {
        if (gtk_window_is_maximized(GTK_WINDOW(app->window))) {
            gtk_window_unmaximize(GTK_WINDOW(app->window));
        } else {
            gtk_window_maximize(GTK_WINDOW(app->window));
        }
        return TRUE;
    }

    if (event->type != GDK_BUTTON_PRESS) {
        return FALSE;
    }

    if (edge != 0) {
        gtk_window_begin_resize_drag(
            GTK_WINDOW(app->window),
            window_edge(edge),
            event->button,
            event->x_root,
            event->y_root,
            event->time);
        return TRUE;
    }

    if (in_titlebar) {
        gtk_window_begin_move_drag(
            GTK_WINDOW(app->window),
            event->button,
            event->x_root,
            event->y_root,
            event->time);
        return TRUE;
    }

    return FALSE;
}

static gboolean on_window_motion(GtkWidget *widget, GdkEventMotion *event, gpointer user_data)
{
    App *app = user_data;
    int width = gtk_widget_get_allocated_width(app->window);
    int height = gtk_widget_get_allocated_height(app->window);
    int edge = edge_from_position(event->x, event->y, width, height);

    (void)widget;

    update_resize_cursor(app, edge);
    return FALSE;
}

static gboolean on_window_leave(GtkWidget *widget, GdkEventCrossing *event, gpointer user_data)
{
    App *app = user_data;

    (void)widget;
    (void)event;
    update_resize_cursor(app, 0);
    return FALSE;
}

static gboolean on_child_motion(GtkWidget *widget, GdkEventMotion *event, gpointer user_data)
{
    App *app = user_data;
    int x;
    int y;
    int width = gtk_widget_get_allocated_width(app->window);
    int height = gtk_widget_get_allocated_height(app->window);

    if (!gtk_widget_translate_coordinates(widget, app->window, event->x, event->y, &x, &y)) {
        return FALSE;
    }

    update_resize_cursor(app, edge_from_position(x, y, width, height));
    return FALSE;
}

static gboolean is_executable(const char *path)
{
    return path != NULL && *path != '\0' && g_file_test(path, G_FILE_TEST_IS_EXECUTABLE);
}

static char *detect_shell(const char *configured)
{
    const char *environment_shell;
    struct passwd *entry;

    if (is_executable(configured)) {
        return g_strdup(configured);
    }

    environment_shell = g_getenv("SHELL");
    if (is_executable(environment_shell)) {
        return g_strdup(environment_shell);
    }

    entry = getpwuid(getuid());
    if (entry != NULL && is_executable(entry->pw_shell)) {
        return g_strdup(entry->pw_shell);
    }

    if (is_executable("/bin/bash")) {
        return g_strdup("/bin/bash");
    }

    return g_strdup("/bin/sh");
}

static void on_error_response(GtkDialog *dialog, gint response, gpointer user_data)
{
    App *app = user_data;

    (void)response;
    gtk_widget_destroy(GTK_WIDGET(dialog));

    if (app->window != NULL) {
        gtk_widget_destroy(app->window);
    }
}

static void on_spawn_done(VteTerminal *terminal, GPid pid, GError *error, gpointer user_data)
{
    SpawnData *data = user_data;
    App *app = data->app;

    (void)terminal;
    (void)pid;

    g_strfreev(data->environment);
    g_free(data->directory);
    g_free(data);

    if (app == NULL) {
        if (error != NULL) {
            g_error_free(error);
        }
        return;
    }

    app->spawn = NULL;

    if (error != NULL) {
        GtkWidget *dialog = gtk_message_dialog_new(
            GTK_WINDOW(app->window),
            GTK_DIALOG_MODAL,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_CLOSE,
            "Unable to start terminal: %s",
            error->message);

        gtk_window_set_title(GTK_WINDOW(dialog), APP_NAME);
        g_signal_connect(dialog, "response", G_CALLBACK(on_error_response), app);
        gtk_widget_show(dialog);
        g_error_free(error);
    }
}

static void spawn_terminal(App *app, const char *const *command)
{
    VteTerminal *terminal = VTE_TERMINAL(app->terminal);
    char **arguments;
    char **environment;
    char *directory;
    SpawnData *data;

    if (command != NULL) {
        arguments = g_strdupv((char **)command);
    } else {
        arguments = g_new0(char *, 2);
        arguments[0] = detect_shell(app->config.shell);
    }

    environment = g_get_environ();
    environment = g_environ_setenv(environment, "TERM", "xterm-256color", TRUE);
    environment = g_environ_setenv(environment, "COLORTERM", "truecolor", TRUE);
    directory = g_get_current_dir();

    data = g_new0(SpawnData, 1);
    data->app = app;
    data->environment = environment;
    data->directory = directory;
    app->spawn = data;

    vte_terminal_spawn_async(
        terminal,
        VTE_PTY_DEFAULT,
        directory,
        arguments,
        environment,
        G_SPAWN_SEARCH_PATH,
        NULL,
        NULL,
        NULL,
        -1,
        NULL,
        on_spawn_done,
        data);

    g_strfreev(arguments);
}

static void on_child_exited(VteTerminal *terminal, gint status, gpointer user_data)
{
    App *app = user_data;

    (void)terminal;
    (void)status;

    if (app->window != NULL) {
        gtk_widget_destroy(app->window);
    }
}

static void on_title_changed(VteTerminal *terminal, gpointer user_data)
{
    App *app = user_data;

    set_window_title(app, vte_terminal_get_window_title(terminal));
}

static void on_window_destroy(GtkWidget *widget, gpointer user_data)
{
    App *app = user_data;

    (void)widget;

    if (app->spawn != NULL) {
        app->spawn->app = NULL;
    }

    app->window = NULL;
    app->terminal = NULL;
    app->scrollbar = NULL;
    app->titlebar = NULL;
    app->title_label = NULL;
    window_count--;

    if (window_count == 0) {
        gtk_main_quit();
    }
}

static void app_free(gpointer data)
{
    App *app = data;

    config_clear(&app->config);
    g_free(app);
}

static void app_copy(App *app)
{
    vte_terminal_copy_clipboard_format(VTE_TERMINAL(app->terminal), VTE_FORMAT_TEXT);
}

static void app_paste(App *app)
{
    vte_terminal_paste_clipboard(VTE_TERMINAL(app->terminal));
}

static void app_paste_primary(App *app)
{
    vte_terminal_paste_primary(VTE_TERMINAL(app->terminal));
}

static void app_select_all(App *app)
{
    vte_terminal_select_all(VTE_TERMINAL(app->terminal));
}

static void app_close(App *app)
{
    if (app->window != NULL) {
        gtk_widget_destroy(app->window);
    }
}

static void app_new_window(App *app)
{
    create_app(&app->config, NULL);
}

static void app_zoom(App *app, double change)
{
    app->font_scale = CLAMP(app->font_scale + change, 0.5, 3.0);
    vte_terminal_set_font_scale(VTE_TERMINAL(app->terminal), app->font_scale);
}

static void app_zoom_reset(App *app)
{
    app->font_scale = 1.0;
    vte_terminal_set_font_scale(VTE_TERMINAL(app->terminal), app->font_scale);
}

static void app_scroll(App *app, double amount)
{
    GtkAdjustment *adjustment = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(app->terminal));
    double page = gtk_adjustment_get_page_size(adjustment);
    double lower = gtk_adjustment_get_lower(adjustment);
    double upper = gtk_adjustment_get_upper(adjustment) - page;
    double value = gtk_adjustment_get_value(adjustment) + amount * page;

    if (upper < lower) {
        upper = lower;
    }

    gtk_adjustment_set_value(adjustment, CLAMP(value, lower, upper));
}

static void app_fullscreen(App *app)
{
    GdkWindow *gdk_window = gtk_widget_get_window(app->window);

    if (gdk_window != NULL && (gdk_window_get_state(gdk_window) & GDK_WINDOW_STATE_FULLSCREEN) != 0) {
        gtk_window_unfullscreen(GTK_WINDOW(app->window));
        if (app->titlebar != NULL) {
            gtk_widget_show(app->titlebar);
        }
    } else {
        gtk_window_fullscreen(GTK_WINDOW(app->window));
        if (app->titlebar != NULL) {
            gtk_widget_hide(app->titlebar);
        }
    }
}

static void on_minimize_clicked(GtkButton *button, gpointer user_data)
{
    App *app = user_data;

    (void)button;
    gtk_window_iconify(GTK_WINDOW(app->window));
}

static void on_maximize_clicked(GtkButton *button, gpointer user_data)
{
    App *app = user_data;
    GtkWindow *window = GTK_WINDOW(app->window);

    (void)button;

    if (gtk_window_is_maximized(window)) {
        gtk_window_unmaximize(window);
    } else {
        gtk_window_maximize(window);
    }
}

static void on_close_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    app_close(user_data);
}

static void menu_show(GtkWidget *menu, gpointer user_data)
{
    App *app = user_data;

    (void)menu;
    gtk_widget_set_sensitive(
        app->menu_copy,
        vte_terminal_get_has_selection(VTE_TERMINAL(app->terminal)));
}

static void on_menu_copy(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    app_copy(user_data);
}

static void on_menu_paste(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    app_paste(user_data);
}

static void on_menu_select_all(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    app_select_all(user_data);
}

static void on_menu_new_window(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    app_new_window(user_data);
}

static void on_menu_close(GtkMenuItem *item, gpointer user_data)
{
    (void)item;
    app_close(user_data);
}

static GtkWidget *build_menu(App *app)
{
    GtkWidget *menu = gtk_menu_new();
    GtkWidget *copy = gtk_menu_item_new_with_label("Copy");
    GtkWidget *paste = gtk_menu_item_new_with_label("Paste");
    GtkWidget *select_all = gtk_menu_item_new_with_label("Select All");
    GtkWidget *separator = gtk_separator_menu_item_new();
    GtkWidget *new_window = gtk_menu_item_new_with_label("New Window");
    GtkWidget *close = gtk_menu_item_new_with_label("Close");

    app->menu_copy = copy;

    g_signal_connect(copy, "activate", G_CALLBACK(on_menu_copy), app);
    g_signal_connect(paste, "activate", G_CALLBACK(on_menu_paste), app);
    g_signal_connect(select_all, "activate", G_CALLBACK(on_menu_select_all), app);
    g_signal_connect(new_window, "activate", G_CALLBACK(on_menu_new_window), app);
    g_signal_connect(close, "activate", G_CALLBACK(on_menu_close), app);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), copy);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), paste);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), select_all);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), separator);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), new_window);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), close);

    gtk_menu_attach_to_widget(GTK_MENU(menu), app->terminal, NULL);
    g_signal_connect(menu, "show", G_CALLBACK(menu_show), app);
    gtk_widget_show_all(menu);

    return menu;
}

static gboolean on_terminal_button(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    App *app = user_data;

    (void)widget;

    if (event->type != GDK_BUTTON_PRESS) {
        return FALSE;
    }

    if (event->button == 2) {
        app_paste_primary(app);
        return TRUE;
    }

    if (event->button == 3) {
        gtk_menu_popup_at_pointer(GTK_MENU(app->menu), (GdkEvent *)event);
        return TRUE;
    }

    return FALSE;
}

static gboolean on_terminal_key(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
    App *app = user_data;
    guint modifiers = event->state & gtk_accelerator_get_default_mod_mask();

    (void)widget;

    if (modifiers == (GDK_CONTROL_MASK | GDK_SHIFT_MASK)) {
        switch (event->keyval) {
        case GDK_KEY_c:
            app_copy(app);
            return TRUE;
        case GDK_KEY_v:
            app_paste(app);
            return TRUE;
        case GDK_KEY_a:
            app_select_all(app);
            return TRUE;
        case GDK_KEY_n:
            app_new_window(app);
            return TRUE;
        case GDK_KEY_w:
            app_close(app);
            return TRUE;
        case GDK_KEY_plus:
        case GDK_KEY_equal:
            app_zoom(app, 0.1);
            return TRUE;
        case GDK_KEY_minus:
        case GDK_KEY_underscore:
            app_zoom(app, -0.1);
            return TRUE;
        case GDK_KEY_0:
        case GDK_KEY_parenright:
            app_zoom_reset(app);
            return TRUE;
        default:
            break;
        }
    }

    if (modifiers == GDK_SHIFT_MASK) {
        if (event->keyval == GDK_KEY_Page_Up) {
            app_scroll(app, -1.0);
            return TRUE;
        }

        if (event->keyval == GDK_KEY_Page_Down) {
            app_scroll(app, 1.0);
            return TRUE;
        }
    }

    if (modifiers == 0 && event->keyval == GDK_KEY_F11) {
        app_fullscreen(app);
        return TRUE;
    }

    return FALSE;
}

static GtkWidget *create_title_button(const char *label, GCallback callback, App *app)
{
    GtkWidget *button = gtk_button_new_with_label(label);

    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_widget_set_focus_on_click(button, FALSE);
    gtk_widget_set_can_focus(button, FALSE);
    gtk_widget_set_size_request(button, 18, 18);
    gtk_widget_set_valign(button, GTK_ALIGN_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(button), "glarp-title-button");
    g_signal_connect(button, "clicked", callback, app);

    return button;
}

static void create_app(const GlarpConfig *config, const char *const *command)
{
    App *app = g_new0(App, 1);
    GdkGeometry geometry;
    GtkWidget *outer;
    GtkWidget *controls;
    GtkWidget *content;
    GtkWidget *minimize;
    GtkWidget *maximize;
    GtkWidget *close;

    config_copy(&app->config, config);
    app->font_scale = 1.0;

    app->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    window_count++;
    g_object_set_data_full(G_OBJECT(app->window), "g-larpterm-app", app, app_free);
    g_signal_connect(app->window, "destroy", G_CALLBACK(on_window_destroy), app);

    gtk_window_set_decorated(GTK_WINDOW(app->window), FALSE);
    gtk_window_set_resizable(GTK_WINDOW(app->window), TRUE);
    gtk_window_set_title(GTK_WINDOW(app->window), APP_NAME);
    gtk_window_set_icon_name(GTK_WINDOW(app->window), APP_ID);
    gtk_window_set_default_size(GTK_WINDOW(app->window), 1024, 640);

    geometry.min_width = 320;
    geometry.min_height = 200;
    gtk_window_set_geometry_hints(
        GTK_WINDOW(app->window), NULL, &geometry, GDK_HINT_MIN_SIZE);

    app->terminal = vte_terminal_new();
    vte_terminal_set_size(VTE_TERMINAL(app->terminal), 80, 24);
    vte_terminal_set_scrollback_lines(VTE_TERMINAL(app->terminal), app->config.scrollback);
    vte_terminal_set_cursor_blink_mode(VTE_TERMINAL(app->terminal), VTE_CURSOR_BLINK_ON);
    vte_terminal_set_cursor_shape(VTE_TERMINAL(app->terminal), VTE_CURSOR_SHAPE_BLOCK);
    vte_terminal_set_audible_bell(VTE_TERMINAL(app->terminal), FALSE);
    vte_terminal_set_mouse_autohide(VTE_TERMINAL(app->terminal), TRUE);
    vte_terminal_set_bold_is_bright(VTE_TERMINAL(app->terminal), TRUE);
    apply_colors(VTE_TERMINAL(app->terminal));
    apply_font(app);

    gtk_widget_set_margin_start(app->terminal, 6);
    gtk_widget_set_margin_end(app->terminal, 4);
    gtk_widget_set_margin_top(app->terminal, 4);
    gtk_widget_set_margin_bottom(app->terminal, 4);

    app->scrollbar = gtk_scrollbar_new(
        GTK_ORIENTATION_VERTICAL,
        gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(app->terminal)));
    gtk_widget_set_size_request(app->scrollbar, 10, -1);
    gtk_widget_set_can_focus(app->scrollbar, FALSE);

    app->titlebar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_size_request(app->titlebar, -1, TITLEBAR_HEIGHT);
    gtk_style_context_add_class(gtk_widget_get_style_context(app->titlebar), "glarp-titlebar");

    app->title_label = gtk_label_new(APP_NAME);
    gtk_label_set_xalign(GTK_LABEL(app->title_label), 0.0);
    gtk_label_set_ellipsize(GTK_LABEL(app->title_label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_valign(app->title_label, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start(app->title_label, 6);
    gtk_widget_set_margin_end(app->title_label, 6);
    gtk_style_context_add_class(gtk_widget_get_style_context(app->title_label), "glarp-title-label");
    style_title_label(app->title_label);

    controls = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_set_valign(controls, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_end(controls, 2);
    minimize = create_title_button("-", G_CALLBACK(on_minimize_clicked), app);
    maximize = create_title_button("+", G_CALLBACK(on_maximize_clicked), app);
    close = create_title_button("X", G_CALLBACK(on_close_clicked), app);
    gtk_box_pack_start(GTK_BOX(controls), minimize, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(controls), maximize, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(controls), close, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(app->titlebar), app->title_label, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(app->titlebar), controls, FALSE, FALSE, 0);

    content = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(content), "glarp-terminal-frame");
    gtk_box_pack_start(GTK_BOX(content), app->terminal, TRUE, TRUE, 0);
    gtk_box_pack_end(GTK_BOX(content), app->scrollbar, FALSE, FALSE, 0);

    outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(outer), "glarp-window-frame");
    gtk_box_pack_start(GTK_BOX(outer), app->titlebar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(outer), content, TRUE, TRUE, 0);
    gtk_container_add(GTK_CONTAINER(app->window), outer);

    app->menu = build_menu(app);

    gtk_widget_add_events(
        app->window,
        GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK);
    gtk_widget_add_events(app->terminal, GDK_POINTER_MOTION_MASK);
    gtk_widget_add_events(app->scrollbar, GDK_POINTER_MOTION_MASK);

    g_signal_connect(app->window, "button-press-event", G_CALLBACK(on_window_button), app);
    g_signal_connect(app->window, "motion-notify-event", G_CALLBACK(on_window_motion), app);
    g_signal_connect(app->window, "leave-notify-event", G_CALLBACK(on_window_leave), app);
    g_signal_connect(app->terminal, "motion-notify-event", G_CALLBACK(on_child_motion), app);
    g_signal_connect(app->scrollbar, "motion-notify-event", G_CALLBACK(on_child_motion), app);

    g_signal_connect(app->terminal, "child-exited", G_CALLBACK(on_child_exited), app);
    g_signal_connect(app->terminal, "window-title-changed", G_CALLBACK(on_title_changed), app);
    g_signal_connect(app->terminal, "button-press-event", G_CALLBACK(on_terminal_button), app);
    g_signal_connect(app->terminal, "key-press-event", G_CALLBACK(on_terminal_key), app);

    gtk_widget_show_all(app->window);
    gtk_widget_grab_focus(app->terminal);

    spawn_terminal(app, command);
}

static void print_help(void)
{
    printf("Usage: %s [-e command [args...]]\n", APP_ID);
    printf("\n");
    printf("Options:\n");
    printf("  -e command   run command instead of the configured shell\n");
    printf("  --help       show this help\n");
    printf("  --version    show version\n");
}

int main(int argc, char **argv)
{
    const char *const *command = NULL;
    GlarpConfig config;
    int index;

    setlocale(LC_ALL, "");

    for (index = 1; index < argc; index++) {
        if (g_strcmp0(argv[index], "-e") == 0) {
            if (index + 1 >= argc) {
                fprintf(stderr, "%s: -e requires a command\n", APP_ID);
                return 2;
            }
            command = (const char *const *)&argv[index + 1];
            break;
        }

        if (g_strcmp0(argv[index], "--help") == 0) {
            print_help();
            return 0;
        }

        if (g_strcmp0(argv[index], "--version") == 0) {
            printf("%s %s\n", APP_NAME, APP_VERSION);
            return 0;
        }

        fprintf(stderr, "%s: unknown option: %s\n", APP_ID, argv[index]);
        return 2;
    }

    g_set_prgname(APP_ID);
    g_set_application_name(APP_NAME);

    gtk_init(&argc, &argv);
    gdk_set_program_class(APP_NAME);
    gtk_window_set_default_icon_name(APP_ID);
    pixel_font_ready = load_pixel_font();
    install_theme();

    config_load(&config);
    create_app(&config, command);
    config_clear(&config);

    gtk_main();
    return 0;
}
