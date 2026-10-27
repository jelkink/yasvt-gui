#include "wordlist.h"

#include <string.h>

#include "csv.h"
#include "speech.h"

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

static int compare_names(gconstpointer a, gconstpointer b)
{
    return g_strcmp0(*(const char *const *)a, *(const char *const *)b);
}

static char *language_name(const char *text)
{
    char *normalized = text_normalize(text);
    char *lower = g_utf8_strdown(normalized, -1);
    g_free(normalized);
    return lower;
}

/* Languages with audio, plus those yasvt knows by name without audio. */
static const char *const extra_languages[] = {"esperanto"};

static gboolean is_comment(const char *line)
{
    while (*line == ' ' || *line == '\t')
        line++;
    return g_str_has_prefix(line, "//");
}

/* A tab on the first line that is not blank or a comment makes the file tab-separated; otherwise
 * picks the more frequent of ',' and ';' on that line, ignoring quoted text. */
static char detect_delimiter(const char *data)
{
    const char *line = data;
    while (*line != '\0') {
        const char *end = line + strcspn(line, "\r\n");
        gboolean blank = TRUE;
        for (const char *p = line; p < end; p++)
            if (!g_ascii_isspace(*p))
                blank = FALSE;
        if (!blank && !is_comment(line))
            break;
        line = end + strspn(end, "\r\n");
    }

    int commas = 0;
    int semicolons = 0;
    gboolean quoted = FALSE;

    for (const char *p = line; *p != '\0' && (quoted || (*p != '\n' && *p != '\r')); p++) {
        if (*p == '\t' && !quoted)
            return '\t';
        if (*p == '"')
            quoted = !quoted;
        else if (!quoted && *p == ',')
            commas++;
        else if (!quoted && *p == ';')
            semicolons++;
    }
    return semicolons > commas ? ';' : ',';
}

/* Romanisation such as "ai4{oi3}" or "hoi2 joeng4": letters with tone digits. */
static gboolean is_romanisation(const char *text)
{
    gboolean digit = FALSE;
    for (const char *p = text; *p != '\0'; p = g_utf8_next_char(p)) {
        gunichar c = g_utf8_get_char(p);
        if (c >= '1' && c <= '6')
            digit = TRUE;
        else if (!(c < 128 && (g_ascii_isalpha((char)c) || strchr(" '/:{}-", (char)c) != NULL)) &&
                 c != 0xFC /* ü */)
            return FALSE;
    }
    return digit;
}

/* Returns the cells of a row as "typed, shown[, note]", or NULL for blank and comment lines. In
 * tab-separated files empty cells are dropped, and in rows "word, romanisation, meaning" (as Pleco
 * exports them) the romanisation becomes the note. */
static char **clean_row(char **row, char delimiter)
{
    guint columns = g_strv_length(row);
    if (columns == 0 || is_comment(row[0]))
        return NULL;

    GPtrArray *cells = g_ptr_array_new();
    for (guint i = 0; i < columns; i++)
        if (delimiter != '\t' || *g_strstrip(row[i]) != '\0')
            g_ptr_array_add(cells, g_strdup(row[i]));

    if (cells->len == 0 || (cells->len == 1 && *g_strstrip((char *)cells->pdata[0]) == '\0')) {
        g_ptr_array_free(cells, TRUE);
        return NULL;
    }
    if (delimiter == '\t' && cells->len >= 3 && is_romanisation(cells->pdata[1])) {
        gpointer romanisation = cells->pdata[1];
        cells->pdata[1] = cells->pdata[2];
        cells->pdata[2] = romanisation;
    }
    g_ptr_array_add(cells, NULL);
    return (char **)g_ptr_array_free(cells, FALSE);
}

/* Reads a vocabulary file into its non-blank, non-comment rows, each with at least one cell. */
static GPtrArray *read_rows(const char *path, GError **error)
{
    char *contents = NULL;
    gsize length = 0;

    if (!g_file_get_contents(path, &contents, &length, error))
        return NULL;

    if (!g_utf8_validate(contents, (gssize)length, NULL)) {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL, "%s is not valid UTF-8", path);
        g_free(contents);
        return NULL;
    }

    const char *data = contents;
    if (g_str_has_prefix(data, "\xEF\xBB\xBF"))
        data += 3;

    char delimiter = detect_delimiter(data);
    GPtrArray *raw = csv_parse(data, delimiter);
    GPtrArray *rows = g_ptr_array_new_with_free_func((GDestroyNotify)g_strfreev);
    for (guint i = 0; i < raw->len; i++) {
        char **row = clean_row(g_ptr_array_index(raw, i), delimiter);
        if (row != NULL)
            g_ptr_array_add(rows, row);
    }
    g_ptr_array_unref(raw);
    g_free(contents);
    return rows;
}

gboolean wordlist_is_known_language(const char *name)
{
    char *language = language_name(name);
    gboolean known = speech_has_language(language);
    for (gsize i = 0; i < G_N_ELEMENTS(extra_languages) && !known; i++)
        known = strcmp(extra_languages[i], language) == 0;
    g_free(language);
    return known;
}

const char *const *wordlist_known_languages(void)
{
    static GPtrArray *names;
    if (names == NULL) {
        names = g_ptr_array_new();
        for (guint i = 0; i < speech_language_count(); i++)
            g_ptr_array_add(names, (gpointer)speech_language_name(i));
        for (gsize i = 0; i < G_N_ELEMENTS(extra_languages); i++)
            g_ptr_array_add(names, (gpointer)extra_languages[i]);
        g_ptr_array_sort(names, compare_names);
        g_ptr_array_add(names, NULL);
    }
    return (const char *const *)names->pdata;
}

const char *wordlist_guess_language(const char *text)
{
    for (const char *p = text; *p != '\0'; p = g_utf8_next_char(p)) {
        switch (g_unichar_get_script(g_utf8_get_char(p))) {
        case G_UNICODE_SCRIPT_HAN:
            return "chinese";
        case G_UNICODE_SCRIPT_CYRILLIC:
            return "russian";
        case G_UNICODE_SCRIPT_ARABIC:
            return "arabic";
        case G_UNICODE_SCRIPT_GREEK:
            return "greek";
        case G_UNICODE_SCRIPT_HANGUL:
            return "korean";
        case G_UNICODE_SCRIPT_HIRAGANA:
        case G_UNICODE_SCRIPT_KATAKANA:
            return "japanese";
        case G_UNICODE_SCRIPT_DEVANAGARI:
            return "hindi";
        default:
            break;
        }
    }
    return NULL;
}

char **wordlist_read_first_row(const char *path, GError **error)
{
    GPtrArray *rows = read_rows(path, error);
    if (rows == NULL)
        return NULL;

    char **first = NULL;
    if (rows->len == 0)
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL, "%s contains no words", path);
    else
        first = g_strdupv(g_ptr_array_index(rows, 0));
    g_ptr_array_unref(rows);
    return first;
}

gboolean wordlist_load_file(WordList *list, const char *path, int start, int number,
                            const char *target_language, const char *source_language,
                            gboolean header, GError **error)
{
    gint64 data_row = 0;
    GPtrArray *rows = read_rows(path, error);
    if (rows == NULL)
        return FALSE;

    gboolean ok = TRUE;
    if (target_language == NULL)
        header = TRUE;
    char *source = source_language != NULL ? language_name(source_language) : NULL;
    char *target = target_language != NULL ? language_name(target_language) : NULL;

    for (guint i = 0; i < rows->len && ok; i++) {
        char **row = g_ptr_array_index(rows, i);
        guint columns = g_strv_length(row);

        if (i == 0 && header) {
            if (source == NULL && columns < 2) {
                g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                            "%s: the first row must name two languages", path);
                ok = FALSE;
            } else if (source == NULL) {
                source = language_name(row[1]);
                target = language_name(row[0]);
            }
            continue;
        }
        if (columns < 2 || source == NULL) {
            g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                        "%s: \"%s\" has only one column, two are needed", path, row[0]);
            ok = FALSE;
        } else {
            data_row++;
            if (data_row < start || (number > 0 && data_row >= (gint64)start + number))
                continue;

            const char *note = columns > 2 ? row[2] : NULL;
            g_ptr_array_add(list->words, word_new(source, target, row[1], row[0], note));
        }
    }

    g_free(source);
    g_free(target);
    g_ptr_array_unref(rows);
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
