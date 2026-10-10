#ifndef WORDLIST_H
#define WORDLIST_H

#include <glib.h>

#include "word.h"

typedef struct {
    GPtrArray *words; /* Word *, the first entry is the current word */
} WordList;

WordList *wordlist_new(void);
void wordlist_free(WordList *list);

/* The first row is a header "source,target[,note]" naming the languages. */
gboolean wordlist_load_file(WordList *list, const char *path, GError **error);

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
