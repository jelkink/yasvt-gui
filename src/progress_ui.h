#ifndef PROGRESS_UI_H
#define PROGRESS_UI_H

#include <gtk/gtk.h>

#include "progress.h"

/* Called after the editor changed or removed an entry, so that the running session can follow. */
typedef struct {
    void (*edited)(gpointer data, const ProgressItem *before, const ProgressItem *after);
    void (*removed)(gpointer data, const ProgressItem *item);
    gpointer data;
} ProgressEditorHooks;

void progress_stats_open(GtkWindow *parent, const Progress *progress);
void progress_editor_open(GtkWindow *parent, Progress *progress, const ProgressEditorHooks *hooks);

#endif
