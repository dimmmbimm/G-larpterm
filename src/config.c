#include "config.h"

#include <glib.h>

#define DEFAULT_SCROLLBACK 10000
#define MIN_SCROLLBACK 100
#define MAX_SCROLLBACK 1000000

static char *config_path(void)
{
    return g_build_filename(g_get_user_config_dir(), "g-larpterm", "config", NULL);
}

static char *read_string(GKeyFile *file, const char *key)
{
    char *value = g_key_file_get_string(file, "g-larpterm", key, NULL);

    if (value != NULL) {
        g_strstrip(value);
        if (*value == '\0') {
            g_free(value);
            value = NULL;
        }
    }

    return value;
}

void config_load(GlarpConfig *config)
{
    GKeyFile *file;
    char *path;
    char *font;
    int scrollback;

    config->shell = NULL;
    config->font = g_strdup(GLARPTERM_DEFAULT_FONT);
    config->scrollback = DEFAULT_SCROLLBACK;

    path = config_path();
    file = g_key_file_new();

    if (g_key_file_load_from_file(file, path, G_KEY_FILE_NONE, NULL)) {
        config->shell = read_string(file, "shell");

        font = read_string(file, "font");
        if (font != NULL) {
            g_free(config->font);
            config->font = font;
        }

        scrollback = g_key_file_get_integer(file, "g-larpterm", "scrollback", NULL);
        if (scrollback >= MIN_SCROLLBACK) {
            config->scrollback = MIN(scrollback, MAX_SCROLLBACK);
        }
    }

    g_key_file_free(file);
    g_free(path);
}

void config_clear(GlarpConfig *config)
{
    g_free(config->shell);
    g_free(config->font);
    config->shell = NULL;
    config->font = NULL;
    config->scrollback = 0;
}

void config_copy(GlarpConfig *destination, const GlarpConfig *source)
{
    destination->shell = g_strdup(source->shell);
    destination->font = g_strdup(source->font);
    destination->scrollback = source->scrollback;
}
