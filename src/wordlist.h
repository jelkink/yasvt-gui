#ifndef WORDLIST_H
#define WORDLIST_H

#include <glib.h>

#include "word.h"

typedef struct {
    GPtrArray *words; /* Word *, the first entry is the current word */
} WordList;

WordList *wordlist_new(void);
void wordlist_free(WordList *list);

/* Files are comma-, semicolon- or tab-separated; blank lines and lines starting with "//" are
 * skipped. The first column is what the user types, the second what is shown (Reverse swaps them),
 * an optional third a note. The first row is normally a header "target,source[,note]" naming the
 * languages. */

/* Whether yasvt knows the language by this name (case-insensitive). */
gboolean wordlist_is_known_language(const char *name);

/* The known language names, lowercase and sorted, NULL-terminated. */
const char *const *wordlist_known_languages(void);

/* A language guessed from the script of the text (Han, Cyrillic, ...), or NULL. */
const char *wordlist_guess_language(const char *text);

/* The cells of the file's first row, to tell whether it is a header; free with g_strfreev(). */
char **wordlist_read_first_row(const char *path, GError **error);

/* The given files, with each folder replaced by the vocabulary files (.csv, .tsv, .txt, .lst)
 * in it and its subfolders, in alphabetical order and skipping hidden ones. Free with g_strfreev(). */
char **wordlist_expand_paths(const char *const *paths, guint count);

/* With target_language NULL, the languages are read from the header row. Otherwise they are the
 * given ones, and header says whether the first row is a header to skip. Only words number
 * `start`, `start`+1, ... are loaded (1-based, counted after any header); number <= 0 means all
 * words from start on. A file that fails to load adds no words. */
gboolean wordlist_load_file(WordList *list, const char *path, int start, int number,
                            const char *target_language, const char *source_language,
                            gboolean header, GError **error);

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
