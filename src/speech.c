#include <gio/gio.h>

#include "speech.h"

/* Same languages and Google TTS codes as yasvt-python's speech.py. */
static const struct {
    const char *name;
    const char *code;
} languages[] = {
    {"english", "en"}, {"russian", "ru"}, {"chinese", "zh"}, {"hongkong", "yue"}, {"italian", "it"},
};

static const char *language_code(const char *language)
{
    for (gsize i = 0; i < G_N_ELEMENTS(languages); i++)
        if (g_strcmp0(languages[i].name, language) == 0)
            return languages[i].code;
    return NULL;
}

gboolean speech_has_language(const char *language)
{
    return language_code(language) != NULL;
}

/* Arguments are passed as $1..$3 so the text is never parsed by the shell. */
static const char script[] =
    "[ -f \"$1\" ] || { curl -fsSL -A 'Mozilla/5.0' -G "
    "--data-urlencode \"q=$2\" --data-urlencode \"tl=$3\" "
    "-d ie=UTF-8 -d client=tw-ob "
    "-o \"$1.part\" https://translate.google.com/translate_tts && mv \"$1.part\" \"$1\"; } "
    "&& mpv --really-quiet --no-video \"$1\"";

static GSubprocess *current;

gboolean speech_say(const char *text, const char *language, GError **error)
{
    const char *code = language_code(language);
    if (code == NULL) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                    "Speech not supported for language '%s'", language);
        return FALSE;
    }

    char *dir = g_build_filename(g_get_user_cache_dir(), "yasvt-gui", NULL);
    g_mkdir_with_parents(dir, 0700);
    char *hash = g_compute_checksum_for_string(G_CHECKSUM_MD5, text, -1);
    char *name = g_strdup_printf("%s_%s.mp3", hash, language);
    char *file = g_build_filename(dir, name, NULL);

    if (current != NULL) {
        g_subprocess_force_exit(current);
        g_clear_object(&current);
    }
    current = g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE,
                               error, "sh", "-c", script, "sh", file, text, code, NULL);

    g_free(file);
    g_free(name);
    g_free(hash);
    g_free(dir);
    return current != NULL;
}
