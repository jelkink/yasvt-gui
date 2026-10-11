#ifndef SETTINGS_H
#define SETTINGS_H

#include <glib.h>

/* Persistent user settings, stored in the user config directory. */
typedef struct Settings Settings;

Settings *settings_new(void);
void settings_free(Settings *settings);

/* Native languages get no automatic audio. Names are lowercase, like in the CSV header. */
gboolean settings_is_native(const Settings *settings, const char *language);
void settings_set_native(Settings *settings, const char *language, gboolean native);

#endif
