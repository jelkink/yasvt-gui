#include <gio/gio.h>

#include "speech.h"

/* Same languages and Google TTS codes as yasvt-python's speech.py. */
static const struct {
    const char *name;
    const char *code;
} languages[] = {
    {"english", "en"}, {"russian", "ru"}, {"chinese", "zh"}, {"hongkong", "yue"}, {"italian", "it"},
    {"arabic", "ar"},  {"dutch", "nl"},   {"latin", "la"},   {"indonesian", "id"},
    {"french", "fr"},  {"german", "de"},  {"spanish", "es"}, {"portuguese", "pt"},
    {"japanese", "ja"}, {"korean", "ko"}, {"turkish", "tr"}, {"polish", "pl"},
    {"hindi", "hi"},   {"greek", "el"},   {"swedish", "sv"},
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

guint speech_language_count(void)
{
    return G_N_ELEMENTS(languages);
}

const char *speech_language_name(guint index)
{
    return languages[index].name;
}

/* exec lets force_exit stop mpv itself rather than only the shell. */
static const char script[] =
    "[ -f \"$1\" ] || { curl -fsSL -A 'Mozilla/5.0' -G "
    "--data-urlencode \"q=$2\" --data-urlencode \"tl=$3\" "
    "-d ie=UTF-8 -d client=tw-ob "
    "-o \"$1.part\" https://translate.google.com/translate_tts && mv \"$1.part\" \"$1\"; } "
    "&& exec mpv --really-quiet --no-video \"$1\"";

typedef struct {
    char *file;
    char *text;
    const char *code;
} Item;

static GQueue pending = G_QUEUE_INIT;
static GSubprocess *current;

static void start_next(void);

static void on_finished(GObject *source, GAsyncResult *result, gpointer user_data)
{
    (void)user_data;
    g_subprocess_wait_finish(G_SUBPROCESS(source), result, NULL);
    /* A stopped player finishes later, after current has moved on. */
    if ((gpointer)source != (gpointer)current)
        return;
    g_clear_object(&current);
    start_next();
}

static void start_next(void)
{
    Item *item = g_queue_pop_head(&pending);
    if (item == NULL)
        return;

    current = g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_SILENCE | G_SUBPROCESS_FLAGS_STDERR_SILENCE,
                               NULL, "sh", "-c", script, "sh", item->file, item->text, item->code,
                               NULL);
    g_free(item->file);
    g_free(item->text);
    g_free(item);

    if (current != NULL)
        g_subprocess_wait_async(current, NULL, on_finished, NULL);
    else
        start_next();
}

void speech_stop(void)
{
    Item *item;
    while ((item = g_queue_pop_head(&pending)) != NULL) {
        g_free(item->file);
        g_free(item->text);
        g_free(item);
    }
    if (current != NULL) {
        g_subprocess_force_exit(current);
        g_clear_object(&current);
    }
}

void speech_append(const char *text, const char *language)
{
    const char *code = language_code(language);
    if (code == NULL)
        return;

    char *dir = g_build_filename(g_get_user_cache_dir(), "yasvt-gui", NULL);
    g_mkdir_with_parents(dir, 0700);
    char *hash = g_compute_checksum_for_string(G_CHECKSUM_MD5, text, -1);
    char *name = g_strdup_printf("%s_%s.mp3", hash, language);

    Item *item = g_new0(Item, 1);
    item->file = g_build_filename(dir, name, NULL);
    item->text = g_strdup(text);
    item->code = code;
    g_queue_push_tail(&pending, item);

    g_free(name);
    g_free(hash);
    g_free(dir);

    if (current == NULL)
        start_next();
}

gboolean speech_say(const char *text, const char *language, GError **error)
{
    if (!speech_has_language(language)) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                    "Speech not supported for language '%s'", language);
        return FALSE;
    }

    speech_stop();
    speech_append(text, language);
    return TRUE;
}
