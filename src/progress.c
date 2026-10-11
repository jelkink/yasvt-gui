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
    char *source_language;
    char *target_language;
    char *source;
    char *target;
    char *note; /* may be NULL */
} Entry;

struct Progress {
    GHashTable *entries; /* key string -> Entry * */
    char *path;
};

static void entry_free(gpointer data)
{
    Entry *entry = data;
    g_free(entry->source_language);
    g_free(entry->target_language);
    g_free(entry->source);
    g_free(entry->target);
    g_free(entry->note);
    g_free(entry);
}

static void entry_set_word(Entry *entry, const Word *word)
{
    g_free(entry->source_language);
    g_free(entry->target_language);
    g_free(entry->source);
    g_free(entry->target);
    g_free(entry->note);
    entry->source_language = g_strdup(word->source_language);
    entry->target_language = g_strdup(word->target_language);
    entry->source = g_strdup(word->source);
    entry->target = g_strdup(word->target);
    entry->note = g_strdup(word->note);
}

/* Fields in the progress file are tab separated, so tabs and newlines are escaped. */
static char *escape(const char *text)
{
    GString *out = g_string_new(NULL);
    for (const char *p = text != NULL ? text : ""; *p != '\0'; p++) {
        switch (*p) {
        case '\\': g_string_append(out, "\\\\"); break;
        case '\t': g_string_append(out, "\\t"); break;
        case '\n': g_string_append(out, "\\n"); break;
        case '\r': g_string_append(out, "\\r"); break;
        default: g_string_append_c(out, *p);
        }
    }
    return g_string_free(out, FALSE);
}

static char *unescape(const char *text)
{
    GString *out = g_string_new(NULL);
    for (const char *p = text; *p != '\0'; p++) {
        if (*p != '\\' || p[1] == '\0') {
            g_string_append_c(out, *p);
            continue;
        }
        p++;
        g_string_append_c(out, *p == 't' ? '\t' : *p == 'n' ? '\n' : *p == 'r' ? '\r' : *p);
    }
    return g_string_free(out, FALSE);
}

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
        char **fields = g_strsplit(*line, "\t", -1);
        unsigned long long history = 0;
        double interval = 0.0;

        if (g_strv_length(fields) == 8) {
            history = g_ascii_strtoull(fields[0], NULL, 10);
            interval = g_ascii_strtod(fields[2], NULL);
        }
        if (g_strv_length(fields) != 8 || history > 255 || interval <= 0.0) {
            g_strfreev(fields);
            continue;
        }

        Entry *entry = g_new0(Entry, 1);
        entry->history = (guint8)history;
        entry->last_seen = g_ascii_strtoll(fields[1], NULL, 10);
        entry->interval = interval;
        entry->source_language = unescape(fields[3]);
        entry->target_language = unescape(fields[4]);
        entry->source = unescape(fields[5]);
        entry->target = unescape(fields[6]);
        entry->note = *fields[7] != '\0' ? unescape(fields[7]) : NULL;

        Word *word = word_new(entry->source_language, entry->target_language, entry->source,
                              entry->target, entry->note);
        g_hash_table_replace(progress->entries, word_key(word), entry);
        word_free(word);
        g_strfreev(fields);
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
        char *fields[5] = {escape(entry->source_language), escape(entry->target_language),
                           escape(entry->source), escape(entry->target), escape(entry->note)};
        g_string_append_printf(text, "%u\t%lld\t%g\t%s\t%s\t%s\t%s\t%s\n", entry->history,
                               (long long)entry->last_seen, entry->interval, fields[0], fields[1],
                               fields[2], fields[3], fields[4]);
        for (int i = 0; i < 5; i++)
            g_free(fields[i]);
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
    progress->entries = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, entry_free);
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
    entry_set_word(entry, word);

    /* The interval only grows once per day, however often the word is answered. */
    gboolean new_day = entry->last_seen == 0 || (now - entry->last_seen) >= SECONDS_PER_DAY;
    if (!correct)
        entry->interval = 1.0;
    else if (entry->last_seen == 0)
        entry->interval = 1.0;
    else if (new_day)
        entry->interval = MIN(MAX(entry->interval * 2.0, 1.0), MAX_INTERVAL_DAYS);

    entry->history = (guint8)((entry->history << 1) | (correct ? 1 : 0));
    entry->last_seen = now;
    save(progress);
}

static const Entry *find_entry(const Progress *progress, const Word *word)
{
    char *key = word_key(word);
    const Entry *entry = g_hash_table_lookup(progress->entries, key);
    g_free(key);
    return entry;
}

static double weight(const Entry *entry)
{
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

void progress_register(Progress *progress, const WordList *list)
{
    for (guint i = 0; i < list->words->len; i++) {
        const Word *word = g_ptr_array_index(list->words, i);
        char *key = word_key(word);
        if (g_hash_table_contains(progress->entries, key)) {
            g_free(key);
            continue;
        }

        Entry *entry = g_new0(Entry, 1);
        entry->interval = 1.0;
        entry_set_word(entry, word);
        g_hash_table_insert(progress->entries, key, entry);
    }
    save(progress);
}

guint progress_load_words(const Progress *progress, WordList *list)
{
    GHashTableIter iter;
    gpointer value;

    g_hash_table_iter_init(&iter, progress->entries);
    while (g_hash_table_iter_next(&iter, NULL, &value)) {
        const Entry *entry = value;
        g_ptr_array_add(list->words, word_new(entry->source_language, entry->target_language,
                                              entry->source, entry->target, entry->note));
    }
    return g_hash_table_size(progress->entries);
}

void progress_shuffle(const Progress *progress, WordList *list, double new_fraction)
{
    guint count = list->words->len;
    Keyed *seen = g_new(Keyed, count);
    Word **fresh = g_new(Word *, count);
    guint seen_count = 0;
    guint fresh_count = 0;

    for (guint i = 0; i < count; i++) {
        Word *word = g_ptr_array_index(list->words, i);
        const Entry *entry = find_entry(progress, word);
        if (entry == NULL || entry->last_seen == 0) {
            fresh[fresh_count++] = word;
            continue;
        }
        /* Weighted random order (Efraimidis-Spirakis) with jitter on the weights. */
        double w = weight(entry) * g_random_double_range(0.5, 1.5);
        seen[seen_count].word = word;
        seen[seen_count].key = pow(g_random_double(), 1.0 / w);
        seen_count++;
    }

    qsort(seen, seen_count, sizeof(Keyed), compare_keys_descending);
    for (guint i = fresh_count; i > 1; i--) {
        guint j = (guint)g_random_int_range(0, (gint)i);
        Word *tmp = fresh[i - 1];
        fresh[i - 1] = fresh[j];
        fresh[j] = tmp;
    }

    /* New words are spread through the list at the requested rate. */
    guint next_seen = 0;
    guint next_fresh = 0;
    for (guint out = 0; out < count; out++) {
        gboolean take_fresh = next_fresh < fresh_count &&
                              (next_seen >= seen_count || g_random_double() < new_fraction);
        list->words->pdata[out] = take_fresh ? fresh[next_fresh++] : seen[next_seen++].word;
    }

    g_free(seen);
    g_free(fresh);
}
