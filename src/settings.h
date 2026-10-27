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

/* Whether Chinese is displayed in traditional rather than simplified characters. */
gboolean settings_get_traditional(const Settings *settings);
void settings_set_traditional(Settings *settings, gboolean traditional);

/* Languages the user chose for a file whose first row does not name them, by path. Returns FALSE
 * when none were chosen; target and source are then not set, otherwise free them with g_free(). */
gboolean settings_get_file_languages(const Settings *settings, const char *path, char **target,
                                     char **source, gboolean *header);
void settings_set_file_languages(Settings *settings, const char *path, const char *target,
                                 const char *source, gboolean header);

#endif
