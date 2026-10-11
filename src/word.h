#ifndef WORD_H
#define WORD_H

#include <glib.h>

typedef struct {
    const char *source_language; /* interned, lowercase */
    const char *target_language; /* interned, lowercase */
    char *source;                /* UTF-8, NFC-normalized */
    char *target;                /* UTF-8, NFC-normalized */
    char *note;                  /* third CSV column, may be NULL */
    guint8 history;              /* last 8 results, bit 0 is the most recent */
    gboolean tested;
} Word;

Word *word_new(const char *source_language, const char *target_language,
               const char *source, const char *target, const char *note);
void word_free(Word *word);

void word_update_score(Word *word, gboolean correct);
double word_average_score(const Word *word);

/* Words followed by generated pinyin in parentheses when their language is Chinese. */
char *word_format_source(const Word *word);
char *word_format_target(const Word *word);

/* Swaps source and target, keeping the score history. */
void word_reverse(Word *word);

void word_reset_score(Word *word);

/* Trims and NFC-normalizes so that equivalent Unicode sequences compare equal. */
char *text_normalize(const char *text);

#endif
