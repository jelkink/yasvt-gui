#ifndef SPEECH_H
#define SPEECH_H

#include <glib.h>

gboolean speech_has_language(const char *language);
guint speech_language_count(void);
const char *speech_language_name(guint index);

/* Plays asynchronously via curl (Google TTS, cached) and mpv; replaces any current playback. */
gboolean speech_say(const char *text, const char *language, GError **error);

/* Queues a word after whatever is playing; unsupported languages are ignored. */
void speech_append(const char *text, const char *language);

void speech_stop(void);

#endif
