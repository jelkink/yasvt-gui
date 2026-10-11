#ifndef WORDLIST_H
#define WORDLIST_H

#include <glib.h>

#include "word.h"

typedef struct {
    GPtrArray *words; /* Word *, the first entry is the current word */
} WordList;

WordList *wordlist_new(void);
void wordlist_free(WordList *list);

/* The first row is a header "target,source[,note]" naming the languages: the first column is
 * what the user types, the second is what is shown (Reverse swaps them). */
/* Only words number `start`, `start`+1, ... are loaded (1-based, counted after the
 * header); number <= 0 means all words from start on. */
gboolean wordlist_load_file(WordList *list, const char *path, int start, int number,
                            GError **error);

void wordlist_shuffle(WordList *list);
Word *wordlist_current(const WordList *list);

/* Moves the current word back in the queue; well-known words go further back. */
void wordlist_reinsert_current(WordList *list);

/* Returns a word with the asked source whose target matches the answer, or NULL. */
Word *wordlist_check(const WordList *list, const char *asked, const char *answer);

/* Scores in percent. */
double wordlist_score(const WordList *list);
double wordlist_score_tested(const WordList *list);

#endif
