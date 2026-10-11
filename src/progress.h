#ifndef PROGRESS_H
#define PROGRESS_H

#include <glib.h>

#include "word.h"
#include "wordlist.h"

/* Learning progress kept across sessions in the user data directory. It only
 * influences the initial order of a freshly loaded list. */
typedef struct Progress Progress;

Progress *progress_new(void);
void progress_free(Progress *progress);

/* Records an answer and saves the progress file. */
void progress_record(Progress *progress, const Word *word, gboolean correct);

/* Records words not yet known so that they are available without the file; they still
 * count as new until answered. */
void progress_register(Progress *progress, const WordList *list);

/* Appends every word with recorded progress to the list; returns how many. */
guint progress_load_words(const Progress *progress, WordList *list);

/* Initial order: words already practised, most due and least known first, with never-seen
 * words mixed in at rate `new_fraction` (0..1). */
void progress_shuffle(const Progress *progress, WordList *list, double new_fraction);

#endif
