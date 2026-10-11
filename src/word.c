#include "word.h"

#include <string.h>

#include "hanzi.h"
#include "pinyin.h"

static gboolean show_traditional;

static gboolean is_chinese(const char *language)
{
    return strcmp(language, "chinese") == 0;
}

static char *normalize_for(const char *language, const char *text)
{
    char *normalized = text_normalize(text);
    if (!is_chinese(language))
        return normalized;

    char *simplified = hanzi_to_simplified(normalized);
    g_free(normalized);
    return simplified;
}

char *word_normalize_answer(const char *language, const char *text)
{
    return normalize_for(language, text);
}

void word_set_traditional(gboolean traditional)
{
    show_traditional = traditional;
}

static char *display_text(const char *language, const char *text)
{
    return show_traditional && is_chinese(language) ? hanzi_to_traditional(text) : g_strdup(text);
}

char *word_display_source(const Word *word)
{
    return display_text(word->source_language, word->source);
}

char *word_display_target(const Word *word)
{
    return display_text(word->target_language, word->target);
}

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
    word->source = normalize_for(source_language, source);
    word->target = normalize_for(target_language, target);
    if (note != NULL && *note != '\0')
        word->note = text_normalize(note);
    return word;
}

void word_set_text(Word *word, const char *source, const char *target, const char *note)
{
    g_free(word->source);
    g_free(word->target);
    g_free(word->note);
    word->source = normalize_for(word->source_language, source);
    word->target = normalize_for(word->target_language, target);
    word->note = note != NULL && *note != '\0' ? text_normalize(note) : NULL;
}

void word_free(Word *word)
{
    if (word == NULL)
        return;
    g_free(word->source);
    g_free(word->target);
    g_free(word->note);
    g_free(word);
}

void word_update_score(Word *word, gboolean correct)
{
    word->history = (guint8)((word->history << 1) | (correct ? 1 : 0));
    word->tested = TRUE;
}

static char *with_pinyin(const char *language, const char *text)
{
    char *shown = display_text(language, text);
    char *pinyin = is_chinese(language) ? pinyin_from_text(text) : NULL;
    if (pinyin == NULL)
        return shown;

    char *result = g_strdup_printf("%s (%s)", shown, pinyin);
    g_free(shown);
    g_free(pinyin);
    return result;
}

char *word_format_source(const Word *word)
{
    return with_pinyin(word->source_language, word->source);
}

char *word_format_target(const Word *word)
{
    return with_pinyin(word->target_language, word->target);
}

void word_reverse(Word *word)
{
    const char *language = word->source_language;
    word->source_language = word->target_language;
    word->target_language = language;

    char *text = word->source;
    word->source = word->target;
    word->target = text;
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
