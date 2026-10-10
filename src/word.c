#include "word.h"

char *text_normalize(const char *text)
{
    char *trimmed = g_strstrip(g_strdup(text));
    char *normalized = g_utf8_normalize(trimmed, -1, G_NORMALIZE_NFC);
    g_free(trimmed);
    return normalized;
}

Word *word_new(const char *source_language, const char *target_language,
               const char *source, const char *target, const char *note)
{
    Word *word = g_new0(Word, 1);
    word->source_language = g_intern_string(source_language);
    word->target_language = g_intern_string(target_language);
    word->source = text_normalize(source);
    word->target = text_normalize(target);
    if (note != NULL && *note != '\0')
        word->note = text_normalize(note);
    return word;
}

void word_free(Word *word)
{
    if (word == NULL)
        return;
    g_free(word->source);
    g_free(word->target);
    g_free(word->note);
    g_free(word->pinyin);
    g_free(word);
}

void word_update_score(Word *word, gboolean correct)
{
    word->history = (guint8)((word->history << 1) | (correct ? 1 : 0));
    word->tested = TRUE;
}

char *word_format_source(const Word *word)
{
    if (word->pinyin != NULL && !word->pinyin_on_target)
        return g_strdup_printf("%s (%s)", word->source, word->pinyin);
    return g_strdup(word->source);
}

char *word_format_target(const Word *word)
{
    if (word->pinyin != NULL && word->pinyin_on_target)
        return g_strdup_printf("%s (%s)", word->target, word->pinyin);
    return g_strdup(word->target);
}

void word_reverse(Word *word)
{
    const char *language = word->source_language;
    word->source_language = word->target_language;
    word->target_language = language;

    char *text = word->source;
    word->source = word->target;
    word->target = text;

    word->pinyin_on_target = !word->pinyin_on_target;
}

void word_reset_score(Word *word)
{
    word->history = 0;
}

double word_average_score(const Word *word)
{
    int bits = 0;
    for (int i = 0; i < 8; i++)
        bits += (word->history >> i) & 1;
    return bits / 8.0;
}
