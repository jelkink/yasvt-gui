#include "progress_ui.h"

#include <math.h>

#define EDITOR_LIMIT 200
#define SECONDS_PER_DAY 86400.0

static gboolean on_key(GtkEventControllerKey *controller, guint keyval, guint keycode,
                       GdkModifierType state, gpointer user_data)
{
    (void)controller;
    (void)keycode;
    (void)state;
    if (keyval != GDK_KEY_Escape)
        return FALSE;
    gtk_window_destroy(GTK_WINDOW(user_data));
    return TRUE;
}

static GtkWidget *new_window(GtkWindow *parent, const char *title, int width, int height)
{
    GtkWidget *window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(window), title);
    gtk_window_set_default_size(GTK_WINDOW(window), width, height);
    gtk_window_set_transient_for(GTK_WINDOW(window), parent);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(window), TRUE);

    GtkEventController *keys = gtk_event_controller_key_new();
    g_signal_connect(keys, "key-pressed", G_CALLBACK(on_key), window);
    gtk_widget_add_controller(window, keys);
    return window;
}

static void add_cell(GtkWidget *grid, const char *text, int column, int row, gboolean bold)
{
    GtkWidget *label = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(label), column == 0 ? 0.0f : 1.0f);
    if (bold) {
        PangoAttrList *attrs = pango_attr_list_new();
        pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
        gtk_label_set_attributes(GTK_LABEL(label), attrs);
        pango_attr_list_unref(attrs);
    }
    gtk_grid_attach(GTK_GRID(grid), label, column, row, 1, 1);
}

static void add_stats_row(GtkWidget *grid, int row, const char *name, const ProgressStats *stats,
                          gboolean bold)
{
    char *numbers[7];
    numbers[0] = g_strdup_printf("%u", stats->total);
    numbers[1] = g_strdup_printf("%u", stats->fresh);
    numbers[2] = g_strdup_printf("%u", stats->learning);
    numbers[3] = g_strdup_printf("%u", stats->known);
    numbers[4] = g_strdup_printf("%u", stats->due);
    numbers[5] = g_strdup_printf("%u", stats->today);
    numbers[6] = stats->answered > 0 ? g_strdup_printf("%.0f%%", stats->accuracy * 100.0)
                                     : g_strdup("-");

    add_cell(grid, name, 0, row, bold);
    for (int i = 0; i < 7; i++) {
        add_cell(grid, numbers[i], i + 1, row, bold);
        g_free(numbers[i]);
    }
}

static GtkWidget *stats_grid(const GPtrArray *pairs)
{
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 18);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 4);

    const char *headers[] = {"Shown \u2192 typed", "Words", "New", "Learning", "Known", "Due",
                             "Today", "Accuracy"};
    for (int i = 0; i < 8; i++)
        add_cell(grid, headers[i], i, 0, TRUE);

    ProgressStats total = {0};
    double accuracy_sum = 0.0;
    for (guint i = 0; i < pairs->len; i++) {
        const ProgressPairStats *pair = g_ptr_array_index(pairs, i);
        char *name = g_strdup_printf("%s \u2192 %s", pair->source_language, pair->target_language);
        add_stats_row(grid, (int)i + 1, name, &pair->stats, FALSE);
        g_free(name);

        total.total += pair->stats.total;
        total.fresh += pair->stats.fresh;
        total.learning += pair->stats.learning;
        total.known += pair->stats.known;
        total.due += pair->stats.due;
        total.today += pair->stats.today;
        total.answered += pair->stats.answered;
        accuracy_sum += pair->stats.accuracy * pair->stats.answered;
    }

    if (pairs->len > 1) {
        total.accuracy = total.answered > 0 ? accuracy_sum / total.answered : 0.0;
        add_stats_row(grid, (int)pairs->len + 1, "Total", &total, TRUE);
    }
    return grid;
}

static void add_heading(GtkWidget *box, const char *text)
{
    GtkWidget *label = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_add_css_class(label, "heading");
    gtk_box_append(GTK_BOX(box), label);
}

void progress_stats_open(GtkWindow *parent, const Progress *progress, const WordList *loaded)
{
    GPtrArray *pairs = progress_pair_stats(progress, NULL);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(box, 18);
    gtk_widget_set_margin_bottom(box, 18);
    gtk_widget_set_margin_start(box, 18);
    gtk_widget_set_margin_end(box, 18);

    if (pairs->len == 0) {
        gtk_box_append(GTK_BOX(box), gtk_label_new("No progress recorded yet. Open a vocabulary file to start."));
    } else {
        GPtrArray *loaded_pairs = progress_pair_stats(progress, loaded);
        if (loaded_pairs->len > 0) {
            add_heading(box, "Loaded words");
            gtk_box_append(GTK_BOX(box), stats_grid(loaded_pairs));
        }
        g_ptr_array_unref(loaded_pairs);
        add_heading(box, "All words in your progress");
        gtk_box_append(GTK_BOX(box), stats_grid(pairs));
    }

    GtkWidget *legend = gtk_label_new("Loaded words: the words loaded now, in their current direction. "
                                      "New: never answered. Learning: answered, review interval under 8 days. "
                                      "Known: interval of 8 days or more. Due: ready to be reviewed again. "
                                      "Today: answered today. Accuracy: share correct of the last 8 answers.");
    gtk_label_set_wrap(GTK_LABEL(legend), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(legend), 70);
    gtk_label_set_xalign(GTK_LABEL(legend), 0.0f);
    gtk_widget_add_css_class(legend, "dim-label");
    gtk_box_append(GTK_BOX(box), legend);
    g_ptr_array_unref(pairs);

    GtkWidget *window = new_window(parent, "Statistics", 0, 0);
    gtk_window_set_child(GTK_WINDOW(window), box);
    gtk_window_present(GTK_WINDOW(window));
}

typedef struct {
    Progress *progress;
    ProgressEditorHooks hooks;
    GtkWidget *search;
    GtkWidget *list;
    GtkWidget *status;
} Editor;

typedef struct {
    Editor *editor;
    ProgressItem *item;
    GtkWidget *source_entry;
    GtkWidget *target_entry;
    GtkWidget *note_entry;
} Row;

static void row_free(gpointer data)
{
    Row *row = data;
    progress_item_free(row->item);
    g_free(row);
}

static char *info_text(const ProgressItem *item)
{
    if (item->last_seen == 0)
        return g_strdup("never answered");

    int correct = 0;
    for (int i = 0; i < 8; i++)
        correct += (item->history >> i) & 1;

    double days_left = item->interval - (double)(g_get_real_time() / G_USEC_PER_SEC - item->last_seen) / SECONDS_PER_DAY;
    char *due = days_left <= 0.0 ? g_strdup("due") : g_strdup_printf("due in %.0f d", ceil(days_left));
    char *text = g_strdup_printf("%d/8 correct, interval %.0f d, ease %.2f, %s", correct,
                                 item->interval, item->ease, due);
    g_free(due);
    return text;
}

static void set_row_texts(Row *row)
{
    gtk_editable_set_text(GTK_EDITABLE(row->source_entry), row->item->source);
    gtk_editable_set_text(GTK_EDITABLE(row->target_entry), row->item->target);
    gtk_editable_set_text(GTK_EDITABLE(row->note_entry), row->item->note != NULL ? row->item->note : "");
}

static void commit(Row *row)
{
    Editor *editor = row->editor;
    char *source = g_strstrip(g_strdup(gtk_editable_get_text(GTK_EDITABLE(row->source_entry))));
    char *target = g_strstrip(g_strdup(gtk_editable_get_text(GTK_EDITABLE(row->target_entry))));
    char *note = g_strstrip(g_strdup(gtk_editable_get_text(GTK_EDITABLE(row->note_entry))));

    if (g_strcmp0(source, row->item->source) != 0 || g_strcmp0(target, row->item->target) != 0 ||
        g_strcmp0(note, row->item->note != NULL ? row->item->note : "") != 0) {
        ProgressItem *after = progress_edit(editor->progress, row->item->key, source, target, note);

        if (after == NULL) {
            gtk_label_set_text(GTK_LABEL(editor->status),
                               "Not saved: texts can't be empty, and the words can't match another entry.");
            set_row_texts(row);
        } else {
            if (editor->hooks.edited != NULL)
                editor->hooks.edited(editor->hooks.data, row->item, after);
            progress_item_free(row->item);
            row->item = after;
            set_row_texts(row);
            gtk_label_set_text(GTK_LABEL(editor->status), "Saved.");
        }
    }

    g_free(source);
    g_free(target);
    g_free(note);
}

static void on_entry_activate(GtkEntry *entry, gpointer user_data)
{
    (void)entry;
    commit(user_data);
}

static void on_entry_leave(GtkEventControllerFocus *controller, gpointer user_data)
{
    (void)controller;
    commit(user_data);
}

static void populate(Editor *editor);

static void on_reset_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    Row *row = user_data;
    Editor *editor = row->editor;
    progress_reset(editor->progress, row->item->key);
    populate(editor);
}

static void on_delete_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    Row *row = user_data;
    Editor *editor = row->editor;
    if (editor->hooks.removed != NULL)
        editor->hooks.removed(editor->hooks.data, row->item);
    progress_remove(editor->progress, row->item->key);
    populate(editor);
}

static GtkWidget *new_text_entry(Row *row, const char *placeholder)
{
    GtkWidget *entry = gtk_entry_new();
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_editable_set_width_chars(GTK_EDITABLE(entry), 12);
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), placeholder);
    g_signal_connect(entry, "activate", G_CALLBACK(on_entry_activate), row);

    GtkEventController *focus = gtk_event_controller_focus_new();
    g_signal_connect(focus, "leave", G_CALLBACK(on_entry_leave), row);
    gtk_widget_add_controller(entry, focus);
    return entry;
}

static void populate(Editor *editor)
{
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(editor->list)) != NULL)
        gtk_list_box_remove(GTK_LIST_BOX(editor->list), child);

    guint total = 0;
    GPtrArray *items = progress_search(editor->progress, gtk_editable_get_text(GTK_EDITABLE(editor->search)),
                                       EDITOR_LIMIT, &total);
    g_ptr_array_set_free_func(items, NULL);

    for (guint i = 0; i < items->len; i++) {
        Row *row = g_new0(Row, 1);
        row->editor = editor;
        row->item = g_ptr_array_index(items, i);

        GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_widget_set_margin_top(box, 3);
        gtk_widget_set_margin_bottom(box, 3);
        gtk_widget_set_margin_start(box, 6);
        gtk_widget_set_margin_end(box, 6);

        char *pair = g_strdup_printf("%s \u2192 %s", row->item->source_language, row->item->target_language);
        GtkWidget *pair_label = gtk_label_new(pair);
        g_free(pair);
        gtk_label_set_xalign(GTK_LABEL(pair_label), 0.0f);
        gtk_label_set_width_chars(GTK_LABEL(pair_label), 16);
        gtk_label_set_ellipsize(GTK_LABEL(pair_label), PANGO_ELLIPSIZE_END);

        row->source_entry = new_text_entry(row, "shown word");
        row->target_entry = new_text_entry(row, "typed word");
        row->note_entry = new_text_entry(row, "note");
        set_row_texts(row);

        char *info = info_text(row->item);
        GtkWidget *info_label = gtk_label_new(info);
        g_free(info);
        gtk_widget_add_css_class(info_label, "dim-label");
        gtk_label_set_xalign(GTK_LABEL(info_label), 0.0f);
        gtk_label_set_width_chars(GTK_LABEL(info_label), 34);

        GtkWidget *reset = gtk_button_new_with_label("Reset");
        g_signal_connect(reset, "clicked", G_CALLBACK(on_reset_clicked), row);
        GtkWidget *delete_button = gtk_button_new_with_label("Delete");
        g_signal_connect(delete_button, "clicked", G_CALLBACK(on_delete_clicked), row);

        gtk_box_append(GTK_BOX(box), pair_label);
        gtk_box_append(GTK_BOX(box), row->source_entry);
        gtk_box_append(GTK_BOX(box), row->target_entry);
        gtk_box_append(GTK_BOX(box), row->note_entry);
        gtk_box_append(GTK_BOX(box), info_label);
        gtk_box_append(GTK_BOX(box), reset);
        gtk_box_append(GTK_BOX(box), delete_button);

        g_object_set_data_full(G_OBJECT(box), "row", row, row_free);
        gtk_list_box_append(GTK_LIST_BOX(editor->list), box);
    }
    guint shown = items->len;
    g_ptr_array_unref(items);

    char *status = total > shown
                       ? g_strdup_printf("Showing %u of %u entries; search to narrow down", shown, total)
                       : g_strdup_printf("%u entries", total);
    gtk_label_set_text(GTK_LABEL(editor->status), status);
    g_free(status);
}

static void on_search_changed(GtkSearchEntry *entry, gpointer user_data)
{
    (void)entry;
    populate(user_data);
}

void progress_editor_open(GtkWindow *parent, Progress *progress, const ProgressEditorHooks *hooks)
{
    Editor *editor = g_new0(Editor, 1);
    editor->progress = progress;
    editor->hooks = *hooks;

    editor->search = gtk_search_entry_new();
    gtk_widget_set_hexpand(editor->search, TRUE);
    gtk_search_entry_set_placeholder_text(GTK_SEARCH_ENTRY(editor->search),
                                          "Search words, notes or languages");
    g_signal_connect(editor->search, "search-changed", G_CALLBACK(on_search_changed), editor);
    editor->status = gtk_label_new(NULL);

    GtkWidget *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_append(GTK_BOX(top), editor->search);
    gtk_box_append(GTK_BOX(top), editor->status);

    editor->list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(editor->list), GTK_SELECTION_NONE);
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), editor->list);

    GtkWidget *hint = gtk_label_new("Edit a text and press Enter to save. Reset makes a word count as new "
                                    "again; Delete removes it from your progress.");
    gtk_widget_add_css_class(hint, "dim-label");
    gtk_label_set_xalign(GTK_LABEL(hint), 0.0f);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(box, 12);
    gtk_widget_set_margin_bottom(box, 12);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_box_append(GTK_BOX(box), top);
    gtk_box_append(GTK_BOX(box), scrolled);
    gtk_box_append(GTK_BOX(box), hint);

    GtkWidget *window = new_window(parent, "Edit progress", 1100, 600);
    gtk_window_set_child(GTK_WINDOW(window), box);
    g_object_set_data_full(G_OBJECT(window), "editor", editor, g_free);

    populate(editor);
    gtk_window_present(GTK_WINDOW(window));
}
