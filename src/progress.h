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

/* Weighted random shuffle: unknown, poorly known and long-unseen words tend to come first. */
void progress_shuffle(const Progress *progress, WordList *list);

#endif
