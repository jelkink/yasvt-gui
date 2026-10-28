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

/* Records an answer. Saved by progress_flush(), which the caller runs now and then. */
void progress_record(Progress *progress, const Word *word, gboolean correct);

/* Writes the progress file if there are unsaved answers. */
void progress_flush(Progress *progress);

/* Records words not yet known so that they are available without the file; they still
 * count as new until answered. */
void progress_register(Progress *progress, const WordList *list);

/* Appends every word with recorded progress to the list; returns how many. */
guint progress_load_words(const Progress *progress, WordList *list);

/* Groups of the initial order, in priority order. */
typedef enum {
    PROGRESS_PRACTISED, /* answered in this direction */
    PROGRESS_REVERSE,   /* only answered in the opposite direction */
    PROGRESS_OTHER,     /* only shares a text with a word answered in another pair or direction */
    PROGRESS_NEW,       /* none of its texts was ever answered */
    PROGRESS_GROUP_COUNT,
} ProgressGroup;

/* Initial order: each group is ordered by itself, most due and least known first (new words at
 * random), and the groups are mixed at rates `fractions` (0..1). The practised group's own
 * fraction is ignored: it takes what the others leave. */
void progress_shuffle(const Progress *progress, WordList *list,
                      const double fractions[PROGRESS_GROUP_COUNT]);

/* Counts per language pair. Words never answered are "new"; answered words are "learning" until
 * their review interval reaches 8 days, then "known"; "due" words are ready for review again. */
typedef struct {
    guint total, fresh, learning, known, due, today, answered;
    double accuracy; /* mean share correct of the last 8 answers, over answered words */
} ProgressStats;

typedef struct {
    const char *source_language; /* shown */
    const char *target_language; /* typed */
    ProgressStats stats;
} ProgressPairStats;

/* Array of ProgressPairStats, sorted by language, over all recorded words, or with `only` over the
 * words in that list (each counted once); free with g_ptr_array_unref(). */
GPtrArray *progress_pair_stats(const Progress *progress, const WordList *only);

/* A copy of one progress entry, for the editor. */
typedef struct {
    char *key;
    char *source_language;
    char *target_language;
    char *source;
    char *target;
    char *note; /* may be NULL */
    guint8 history;
    gint64 last_seen; /* 0 when never answered */
    double interval;
    double ease;
} ProgressItem;

void progress_item_free(ProgressItem *item);

/* Entries matching the query in any text field (all if empty), sorted, at most `limit` of them. */
GPtrArray *progress_search(const Progress *progress, const char *query, guint limit,
                           guint *total_matches);

/* Changes the texts of an entry and saves. Returns the updated entry, or NULL if a text is
 * empty or another entry already has these words. */
ProgressItem *progress_edit(Progress *progress, const char *key, const char *source,
                            const char *target, const char *note);

/* Both save immediately. Reset makes the word count as new again. */
void progress_remove(Progress *progress, const char *key);
void progress_reset(Progress *progress, const char *key);

#endif
