#include "progress.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define SECONDS_PER_DAY 86400.0
#define MIN_WEIGHT 0.1
#define MAX_INTERVAL_DAYS 365.0

typedef struct {
    guint8 history; /* last 8 results across sessions, bit 0 is the most recent */
    gint64 last_seen;
    double interval; /* days after which the word counts as due again */
} Entry;

struct Progress {
    GHashTable *entries; /* key string -> Entry * */
    char *path;
};

static char *word_key(const Word *word)
{
    char *text = g_strdup_printf("%s\x1f%s\x1f%s\x1f%s", word->source_language,
                                 word->target_language, word->source, word->target);
    char *key = g_compute_checksum_for_string(G_CHECKSUM_MD5, text, -1);
    g_free(text);
    return key;
}

static void load(Progress *progress)
{
    char *contents = NULL;
    if (!g_file_get_contents(progress->path, &contents, NULL, NULL))
        return;

    char **lines = g_strsplit(contents, "\n", -1);
    for (char **line = lines; *line != NULL; line++) {
        char key[33];
        unsigned history;
        long long last_seen;
        double interval;
        if (sscanf(*line, "%32s %u %lld %lf", key, &history, &last_seen, &interval) != 4 ||
            history > 255 || interval <= 0.0)
            continue;

        Entry *entry = g_new0(Entry, 1);
        entry->history = (guint8)history;
        entry->last_seen = last_seen;
        entry->interval = interval;
        g_hash_table_replace(progress->entries, g_strdup(key), entry);
    }
    g_strfreev(lines);
    g_free(contents);
}

static void save(const Progress *progress)
{
    GString *text = g_string_new(NULL);
    GHashTableIter iter;
    gpointer key, value;

    g_hash_table_iter_init(&iter, progress->entries);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        const Entry *entry = value;
        g_string_append_printf(text, "%s %u %lld %g\n", (const char *)key, entry->history,
                               (long long)entry->last_seen, entry->interval);
    }

    char *dir = g_path_get_dirname(progress->path);
    g_mkdir_with_parents(dir, 0700);
    g_file_set_contents(progress->path, text->str, (gssize)text->len, NULL);
    g_free(dir);
    g_string_free(text, TRUE);
}

Progress *progress_new(void)
{
    Progress *progress = g_new0(Progress, 1);
    progress->entries = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    progress->path = g_build_filename(g_get_user_data_dir(), "yasvt-gui", "progress.txt", NULL);
    load(progress);
    return progress;
}

void progress_free(Progress *progress)
{
    if (progress == NULL)
        return;
    g_hash_table_unref(progress->entries);
    g_free(progress->path);
    g_free(progress);
}

void progress_record(Progress *progress, const Word *word, gboolean correct)
{
    char *key = word_key(word);
    Entry *entry = g_hash_table_lookup(progress->entries, key);
    gint64 now = g_get_real_time() / G_USEC_PER_SEC;

    if (entry == NULL) {
        entry = g_new0(Entry, 1);
        g_hash_table_insert(progress->entries, key, entry);
    } else {
        g_free(key);
    }

    /* The interval only grows once per day, however often the word is answered. */
    gboolean new_day = entry->last_seen == 0 || (now - entry->last_seen) >= SECONDS_PER_DAY;
    if (!correct)
        entry->interval = 1.0;
    else if (new_day)
        entry->interval = MIN(MAX(entry->interval * 2.0, 1.0), MAX_INTERVAL_DAYS);

    entry->history = (guint8)((entry->history << 1) | (correct ? 1 : 0));
    entry->last_seen = now;
    save(progress);
}

static double weight(const Progress *progress, const Word *word)
{
    char *key = word_key(word);
    const Entry *entry = g_hash_table_lookup(progress->entries, key);
    g_free(key);

    if (entry == NULL)
        return 1.0;

    int correct = 0;
    for (int i = 0; i < 8; i++)
        correct += (entry->history >> i) & 1;

    double days = (double)(g_get_real_time() / G_USEC_PER_SEC - entry->last_seen) / SECONDS_PER_DAY;
    double due = MIN(1.0, MAX(days, 0.0) / entry->interval);
    double result = ((1.0 - correct / 8.0) + due) / 2.0;
    return MAX(result, MIN_WEIGHT);
}

typedef struct {
    Word *word;
    double key;
} Keyed;

static int compare_keys_descending(const void *a, const void *b)
{
    double ka = ((const Keyed *)a)->key;
    double kb = ((const Keyed *)b)->key;
    return (ka < kb) - (ka > kb);
}

void progress_shuffle(const Progress *progress, WordList *list)
{
    guint count = list->words->len;
    Keyed *items = g_new(Keyed, count);

    /* Weighted random order (Efraimidis-Spirakis) with jitter on the weights. */
    for (guint i = 0; i < count; i++) {
        Word *word = g_ptr_array_index(list->words, i);
        double w = weight(progress, word) * g_random_double_range(0.5, 1.5);
        items[i].word = word;
        items[i].key = pow(g_random_double(), 1.0 / w);
    }

    qsort(items, count, sizeof(Keyed), compare_keys_descending);
    for (guint i = 0; i < count; i++)
        list->words->pdata[i] = items[i].word;
    g_free(items);
}
