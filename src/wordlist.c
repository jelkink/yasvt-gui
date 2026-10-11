#include "wordlist.h"

#include <string.h>

#include "csv.h"

#define REINSERT_WINDOW 40

WordList *wordlist_new(void)
{
    WordList *list = g_new0(WordList, 1);
    list->words = g_ptr_array_new_with_free_func((GDestroyNotify)word_free);
    return list;
}

void wordlist_free(WordList *list)
{
    if (list == NULL)
        return;
    g_ptr_array_unref(list->words);
    g_free(list);
}

static char *language_name(const char *text)
{
    char *normalized = text_normalize(text);
    char *lower = g_utf8_strdown(normalized, -1);
    g_free(normalized);
    return lower;
}

/* Picks the more frequent of ',' and ';' on the header line, ignoring quoted text. */
static char detect_delimiter(const char *data)
{
    int commas = 0;
    int semicolons = 0;
    gboolean quoted = FALSE;

    for (const char *p = data; *p != '\0' && (quoted || (*p != '\n' && *p != '\r')); p++) {
        if (*p == '"')
            quoted = !quoted;
        else if (!quoted && *p == ',')
            commas++;
        else if (!quoted && *p == ';')
            semicolons++;
    }
    return semicolons > commas ? ';' : ',';
}

gboolean wordlist_load_file(WordList *list, const char *path, int start, int number,
                            GError **error)
{
    char *contents = NULL;
    gsize length = 0;
    gint64 data_row = 0;

    if (!g_file_get_contents(path, &contents, &length, error))
        return FALSE;

    if (!g_utf8_validate(contents, (gssize)length, NULL)) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL, "%s is not valid UTF-8", path);
        g_free(contents);
        return FALSE;
    }

    const char *data = contents;
    if (g_str_has_prefix(data, "\xEF\xBB\xBF"))
        data += 3;

    GPtrArray *rows = csv_parse(data, detect_delimiter(data));
    gboolean ok = TRUE;
    char *source_language = NULL;
    char *target_language = NULL;

    for (guint i = 0; i < rows->len && ok; i++) {
        char **row = g_ptr_array_index(rows, i);
        guint columns = g_strv_length(row);

        if (columns == 1 && *g_strstrip(row[0]) == '\0')
            continue;

        if (columns < 2) {
            g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                        "%s, line %u: expected at least two columns", path, i + 1);
            ok = FALSE;
        } else if (source_language == NULL) {
            source_language = language_name(row[1]);
            target_language = language_name(row[0]);
        } else {
            data_row++;
            if (data_row < start || (number > 0 && data_row >= (gint64)start + number))
                continue;

            const char *note = columns > 2 ? row[2] : NULL;
            g_ptr_array_add(list->words,
                            word_new(source_language, target_language, row[1], row[0], note));
        }
    }

    g_free(source_language);
    g_free(target_language);
    g_ptr_array_unref(rows);
    g_free(contents);
    return ok;
}

void wordlist_shuffle(WordList *list)
{
    for (guint i = list->words->len; i > 1; i--) {
        guint j = (guint)g_random_int_range(0, (gint)i);
        gpointer tmp = list->words->pdata[i - 1];
        list->words->pdata[i - 1] = list->words->pdata[j];
        list->words->pdata[j] = tmp;
    }
}

Word *wordlist_current(const WordList *list)
{
    return list->words->len > 0 ? g_ptr_array_index(list->words, 0) : NULL;
}

void wordlist_reinsert_current(WordList *list)
{
    if (list->words->len < 2)
        return;

    Word *word = g_ptr_array_steal_index(list->words, 0);
    double score = word_average_score(word);
    guint count = list->words->len;
    guint index;

    if (score < 0.99 || count < REINSERT_WINDOW + 1)
        index = (guint)(score * MIN(REINSERT_WINDOW, count));
    else
        index = (guint)g_random_int_range(REINSERT_WINDOW, (gint)count + 1);

    g_ptr_array_insert(list->words, (gint)index, word);
}

Word *wordlist_check(const WordList *list, const char *asked, const char *answer)
{
    Word *found = NULL;

    for (guint i = 0; i < list->words->len && found == NULL; i++) {
        Word *word = g_ptr_array_index(list->words, i);
        if (strcmp(word->source, asked) != 0)
            continue;

        char *normalized = word_normalize_answer(word->target_language, answer);
        if (strcmp(word->target, normalized) == 0)
            found = word;
        g_free(normalized);
    }
    return found;
}

double wordlist_score(const WordList *list)
{
    if (list->words->len == 0)
        return 0.0;

    double sum = 0.0;
    for (guint i = 0; i < list->words->len; i++)
        sum += word_average_score(g_ptr_array_index(list->words, i));
    return sum / list->words->len * 100.0;
}

double wordlist_score_tested(const WordList *list)
{
    double sum = 0.0;
    guint count = 0;

    for (guint i = 0; i < list->words->len; i++) {
        Word *word = g_ptr_array_index(list->words, i);
        if (word->tested) {
            sum += word_average_score(word);
            count++;
        }
    }
    return count > 0 ? sum / count * 100.0 : 0.0;
}
