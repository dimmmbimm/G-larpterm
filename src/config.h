#ifndef GLARPTERM_CONFIG_H
#define GLARPTERM_CONFIG_H

#define GLARPTERM_DEFAULT_FONT "G-Larpterm Pixel 16"

typedef struct {
    char *shell;
    char *font;
    int scrollback;
} GlarpConfig;

void config_load(GlarpConfig *config);
void config_clear(GlarpConfig *config);
void config_copy(GlarpConfig *destination, const GlarpConfig *source);

#endif
