#ifndef SPEECH_H
#define SPEECH_H

#include <glib.h>

gboolean speech_has_language(const char *language);

/* Plays asynchronously via curl (Google TTS, cached) and mpv; replaces any current playback. */
gboolean speech_say(const char *text, const char *language, GError **error);

#endif
