#include <gtk/gtk.h>

#include "gui.h"
#include "progress.h"
#include "settings.h"
#include "speech.h"
#include "wordlist.h"

static int range_start = 1;
static int range_number = -1;
static int new_percent = 20;

typedef enum {
    MODE_TRANSLATE,
    MODE_TYPE,
    MODE_HEAR_TRANSLATE,
    MODE_HEAR_TYPE,
    MODE_COUNT,
} Mode;

typedef struct {
    GtkWindow *window;
    GtkWidget *prompt_label;
    GtkWidget *word_label;
    GtkWidget *entry;
    GtkWidget *feedback_label;
    GtkWidget *score_label;
    GtkWidget *mode_check[MODE_COUNT];
    Mode mode;
    WordList *list; /* the queue: the words that pass the language filter, not owned */
    WordList *all;  /* every loaded word, owns them */
    GHashTable *excluded_source;
    GHashTable *excluded_target;
    GtkWidget *source_box;
    GtkWidget *target_box;
    Progress *progress;
    Settings *settings;
} App;

static const char *const mode_labels[MODE_COUNT] = {
    "Translate", "Type the word", "Hear and translate", "Hear and type",
};

/* Speaks the target word in "hear and type" mode, otherwise the source word. */
static void speak_current(App *app, gboolean replace)
{
    Word *word = wordlist_current(app->list);
    if (word == NULL)
        return;

    gboolean target = app->mode == MODE_HEAR_TYPE;
    const char *language = target ? word->target_language : word->source_language;
    const char *text = target ? word->target : word->source;
    if (!speech_has_language(language))
        return;

    if (!replace) {
        speech_append(text, language);
        return;
    }

    GError *error = NULL;
    if (!speech_say(text, language, &error)) {
        gtk_label_set_text(GTK_LABEL(app->feedback_label), error->message);
        g_error_free(error);
    }
}

/* Native languages are not read out automatically. */
static gboolean audible(const App *app, const char *language)
{
    return speech_has_language(language) && !settings_is_native(app->settings, language);
}

static Mode pick_mode(const App *app, const Word *word)
{
    Mode candidates[MODE_COUNT];
    int count = 0;

    for (int i = 0; i < MODE_COUNT; i++) {
        if (!gtk_check_button_get_active(GTK_CHECK_BUTTON(app->mode_check[i])))
            continue;
        if (i == MODE_HEAR_TRANSLATE && !audible(app, word->source_language))
            continue;
        if (i == MODE_HEAR_TYPE && !audible(app, word->target_language))
            continue;
        candidates[count++] = (Mode)i;
    }
    return count == 0 ? MODE_TRANSLATE : candidates[g_random_int_range(0, count)];
}

static void show_current_word(App *app)
{
    Word *word = wordlist_current(app->list);

    if (word == NULL) {
        gtk_label_set_text(GTK_LABEL(app->prompt_label),
                           app->all->words->len > 0 ? "No words match the language filter"
                                                    : "Open a vocabulary file to start");
        gtk_label_set_text(GTK_LABEL(app->word_label), "");
        gtk_widget_set_sensitive(app->entry, FALSE);
        return;
    }

    app->mode = pick_mode(app, word);
    char *prompt;
    const char *shown;
    switch (app->mode) {
    case MODE_TYPE:
        prompt = g_strdup_printf("Type out the word in %s:", word->target_language);
        shown = word->target;
        break;
    case MODE_HEAR_TRANSLATE:
        prompt = g_strdup_printf("Translate what you hear from %s to %s:", word->source_language,
                                 word->target_language);
        shown = "🔊";
        break;
    case MODE_HEAR_TYPE:
        prompt = g_strdup_printf("Type out the word you hear in %s:", word->target_language);
        shown = "🔊";
        break;
    default:
        prompt = g_strdup_printf("Translate from %s to %s:", word->source_language,
                                 word->target_language);
        shown = word->source;
        break;
    }
    gtk_label_set_text(GTK_LABEL(app->prompt_label), prompt);
    gtk_label_set_text(GTK_LABEL(app->word_label), shown);
    gtk_widget_set_sensitive(app->entry, TRUE);
    gtk_editable_set_text(GTK_EDITABLE(app->entry), "");
    gtk_widget_grab_focus(app->entry);
    g_free(prompt);

    if (app->mode == MODE_HEAR_TRANSLATE || app->mode == MODE_HEAR_TYPE)
        speak_current(app, FALSE);
}

static void show_score(App *app)
{
    char *text = g_strdup_printf("Score: %.1f%% (%.1f%% of those tested)",
                                 wordlist_score(app->list), wordlist_score_tested(app->list));
    gtk_label_set_text(GTK_LABEL(app->score_label), text);
    g_free(text);
}

static char *format_pair(const Word *word)
{
    char *source = word_format_source(word);
    char *target = word_format_target(word);
    char *pair = g_strdup_printf("%s --> %s", source, target);
    g_free(source);
    g_free(target);
    return pair;
}

static void speak_answer(const App *app, const Word *word, gboolean correct)
{
    speech_stop();
    if (correct) {
        if (audible(app, word->target_language))
            speech_append(word->target, word->target_language);
        else if (audible(app, word->source_language))
            speech_append(word->source, word->source_language);
        return;
    }
    if (audible(app, word->source_language))
        speech_append(word->source, word->source_language);
    if (audible(app, word->target_language))
        speech_append(word->target, word->target_language);
}

static void on_answer(GtkEntry *entry, gpointer user_data)
{
    App *app = user_data;
    Word *word = wordlist_current(app->list);
    if (word == NULL)
        return;

    Word *found = wordlist_check(app->list, word->source, gtk_editable_get_text(GTK_EDITABLE(entry)));
    char *feedback;
    char *pair = format_pair(word);

    if (found == word) {
        feedback = g_strdup_printf("CORRECT! %s", pair);
        word_update_score(word, TRUE);
        progress_record(app->progress, word, TRUE);
        speak_answer(app, word, TRUE);
    } else if (found != NULL) {
        feedback = g_strdup_printf("CORRECT! Although I was looking for: %s", pair);
        word_update_score(found, TRUE);
        progress_record(app->progress, found, TRUE);
        speak_answer(app, found, TRUE);
    } else {
        feedback = g_strdup_printf("OOPS! %s%s%s", pair, word->note ? "\nNote: " : "",
                                   word->note ? word->note : "");
        word_update_score(word, FALSE);
        progress_record(app->progress, word, FALSE);
        speak_answer(app, word, FALSE);
    }

    gtk_label_set_text(GTK_LABEL(app->feedback_label), feedback);
    g_free(feedback);
    g_free(pair);

    wordlist_reinsert_current(app->list);
    show_score(app);
    show_current_word(app);
}

static gboolean passes_filter(const App *app, const Word *word)
{
    return !g_hash_table_contains(app->excluded_source, word->source_language) &&
           !g_hash_table_contains(app->excluded_target, word->target_language);
}

static void apply_filter(App *app)
{
    g_ptr_array_set_size(app->list->words, 0);
    for (guint i = 0; i < app->all->words->len; i++) {
        Word *word = g_ptr_array_index(app->all->words, i);
        if (passes_filter(app, word))
            g_ptr_array_add(app->list->words, word);
    }
    progress_shuffle(app->progress, app->list, new_percent / 100.0);
    show_score(app);
    show_current_word(app);
}

static int compare_names(gconstpointer a, gconstpointer b)
{
    return g_strcmp0(*(const char *const *)a, *(const char *const *)b);
}

static void on_filter_toggled(GtkCheckButton *check, gpointer user_data)
{
    App *app = user_data;
    GHashTable *excluded = g_object_get_data(G_OBJECT(check), "excluded");
    const char *language = g_object_get_data(G_OBJECT(check), "language");

    if (gtk_check_button_get_active(check))
        g_hash_table_remove(excluded, language);
    else
        g_hash_table_add(excluded, g_strdup(language));
    apply_filter(app);
}

static void fill_filter_box(App *app, GtkWidget *box, GHashTable *excluded, gboolean source)
{
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(box)) != NULL)
        gtk_box_remove(GTK_BOX(box), child);

    GPtrArray *names = g_ptr_array_new();
    for (guint i = 0; i < app->all->words->len; i++) {
        const Word *word = g_ptr_array_index(app->all->words, i);
        const char *language = source ? word->source_language : word->target_language;
        if (!g_ptr_array_find_with_equal_func(names, language, g_str_equal, NULL))
            g_ptr_array_add(names, (gpointer)language);
    }
    g_ptr_array_sort(names, compare_names);

    for (guint i = 0; i < names->len; i++) {
        const char *name = g_ptr_array_index(names, i);
        char first[8] = {0};
        g_unichar_to_utf8(g_unichar_toupper(g_utf8_get_char(name)), first);
        char *label = g_strconcat(first, g_utf8_next_char(name), NULL);

        GtkWidget *check = gtk_check_button_new_with_label(label);
        g_free(label);
        g_object_set_data_full(G_OBJECT(check), "language", g_strdup(name), g_free);
        g_object_set_data(G_OBJECT(check), "excluded", excluded);
        gtk_check_button_set_active(GTK_CHECK_BUTTON(check), !g_hash_table_contains(excluded, name));
        g_signal_connect(check, "toggled", G_CALLBACK(on_filter_toggled), app);
        gtk_box_append(GTK_BOX(box), check);
    }
    g_ptr_array_free(names, TRUE);
}

static void rebuild_filter_panel(App *app)
{
    fill_filter_box(app, app->source_box, app->excluded_source, TRUE);
    fill_filter_box(app, app->target_box, app->excluded_target, FALSE);
}

static void load_paths(App *app, const char *const *paths, guint count)
{
    g_ptr_array_set_size(app->list->words, 0);
    g_ptr_array_set_size(app->all->words, 0);
    g_hash_table_remove_all(app->excluded_source);
    g_hash_table_remove_all(app->excluded_target);

    for (guint i = 0; i < count; i++) {
        GError *error = NULL;
        if (!wordlist_load_file(app->all, paths[i], range_start, range_number, &error)) {
            gtk_label_set_text(GTK_LABEL(app->feedback_label), error->message);
            g_error_free(error);
            rebuild_filter_panel(app);
            show_score(app);
            show_current_word(app);
            return;
        }
    }

    progress_register(app->progress, app->all);
    rebuild_filter_panel(app);
    char *message = g_strdup_printf("Loaded %u words.", app->all->words->len);
    gtk_label_set_text(GTK_LABEL(app->feedback_label), message);
    g_free(message);
    apply_filter(app);
}

static void on_files_chosen(GObject *source, GAsyncResult *result, gpointer user_data)
{
    App *app = user_data;
    GListModel *files = gtk_file_dialog_open_multiple_finish(GTK_FILE_DIALOG(source), result, NULL);
    if (files == NULL)
        return;

    guint count = g_list_model_get_n_items(files);
    char **paths = g_new0(char *, count + 1);
    for (guint i = 0; i < count; i++) {
        GFile *file = g_list_model_get_item(files, i);
        paths[i] = g_file_get_path(file);
        g_object_unref(file);
    }

    load_paths(app, (const char *const *)paths, count);
    g_strfreev(paths);
    g_object_unref(files);
}

static void on_speak_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    speak_current(user_data, TRUE);
}

static void on_reverse_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    App *app = user_data;
    for (guint i = 0; i < app->all->words->len; i++) {
        Word *word = g_ptr_array_index(app->all->words, i);
        word_reverse(word);
        word_reset_score(word);
        word->tested = FALSE;
    }
    GHashTable *swap = app->excluded_source;
    app->excluded_source = app->excluded_target;
    app->excluded_target = swap;
    rebuild_filter_panel(app);
    apply_filter(app);
}

static void on_shuffle_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    App *app = user_data;
    wordlist_shuffle(app->list);
    show_current_word(app);
}

static void on_clear_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    App *app = user_data;
    Word *word = wordlist_current(app->list);
    if (word == NULL)
        return;
    word_reset_score(word);
    show_score(app);
}

static void on_delete_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    App *app = user_data;
    if (app->list->words->len == 0)
        return;
    Word *word = g_ptr_array_steal_index(app->list->words, 0);
    g_ptr_array_remove(app->all->words, word);
    show_score(app);
    show_current_word(app);
}

static gboolean on_list_key(GtkEventControllerKey *controller, guint keyval, guint keycode,
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

static void on_print_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    App *app = user_data;
    GString *text = g_string_new(NULL);

    for (guint i = 0; i < app->list->words->len; i++) {
        Word *word = g_ptr_array_index(app->list->words, i);
        char *pair = format_pair(word);
        g_string_append_printf(text, "%3.0f%% %s\n", word_average_score(word) * 100.0, pair);
        g_free(pair);
    }

    GtkWidget *view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(view), TRUE);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)), text->str, -1);
    g_string_free(text, TRUE);

    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scrolled, TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), view);

    GtkWidget *window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(window), "Word list");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 500);
    gtk_window_set_transient_for(GTK_WINDOW(window), app->window);

    GtkWidget *close = gtk_button_new_with_label("Close");
    gtk_widget_set_halign(close, GTK_ALIGN_END);
    gtk_widget_set_margin_top(close, 6);
    gtk_widget_set_margin_bottom(close, 6);
    gtk_widget_set_margin_end(close, 6);
    g_signal_connect_swapped(close, "clicked", G_CALLBACK(gtk_window_destroy), window);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(box), scrolled);
    gtk_box_append(GTK_BOX(box), close);
    gtk_window_set_child(GTK_WINDOW(window), box);

    GtkEventController *keys = gtk_event_controller_key_new();
    g_signal_connect(keys, "key-pressed", G_CALLBACK(on_list_key), window);
    gtk_widget_add_controller(window, keys);

    gtk_window_present(GTK_WINDOW(window));
}

static void on_native_toggled(GtkCheckButton *check, gpointer user_data)
{
    App *app = user_data;
    const char *language = g_object_get_data(G_OBJECT(check), "language");
    settings_set_native(app->settings, language, gtk_check_button_get_active(check));
}

static void on_settings_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    App *app = user_data;

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_top(box, 18);
    gtk_widget_set_margin_bottom(box, 18);
    gtk_widget_set_margin_start(box, 18);
    gtk_widget_set_margin_end(box, 18);
    gtk_box_append(GTK_BOX(box), gtk_label_new("Native languages (not read out automatically):"));

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 18);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
    for (guint i = 0; i < speech_language_count(); i++) {
        const char *name = speech_language_name(i);
        char *label = g_strdup(name);
        label[0] = g_ascii_toupper(label[0]);

        GtkWidget *check = gtk_check_button_new_with_label(label);
        g_free(label);
        g_object_set_data(G_OBJECT(check), "language", (gpointer)name);
        gtk_check_button_set_active(GTK_CHECK_BUTTON(check), settings_is_native(app->settings, name));
        g_signal_connect(check, "toggled", G_CALLBACK(on_native_toggled), app);
        gtk_grid_attach(GTK_GRID(grid), check, (int)(i % 3), (int)(i / 3), 1, 1);
    }
    gtk_box_append(GTK_BOX(box), grid);

    GtkWidget *window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(window), "Settings");
    gtk_window_set_transient_for(GTK_WINDOW(window), app->window);
    gtk_window_set_modal(GTK_WINDOW(window), TRUE);
    gtk_window_set_child(GTK_WINDOW(window), box);

    GtkEventController *keys = gtk_event_controller_key_new();
    g_signal_connect(keys, "key-pressed", G_CALLBACK(on_list_key), window);
    gtk_widget_add_controller(window, keys);

    gtk_window_present(GTK_WINDOW(window));
}

static GtkWidget *add_button(GtkWidget *box, const char *label, GCallback callback, App *app)
{
    GtkWidget *button = gtk_button_new_with_label(label);
    g_signal_connect(button, "clicked", callback, app);
    gtk_box_append(GTK_BOX(box), button);
    return button;
}

static void on_open_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    App *app = user_data;
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Open vocabulary files");
    gtk_file_dialog_open_multiple(dialog, app->window, NULL, on_files_chosen, app);
    g_object_unref(dialog);
}

static void app_free(gpointer data)
{
    App *app = data;
    wordlist_free(app->list);
    wordlist_free(app->all);
    g_hash_table_unref(app->excluded_source);
    g_hash_table_unref(app->excluded_target);
    progress_free(app->progress);
    settings_free(app->settings);
    g_free(app);
}

static App *build_window(GtkApplication *gtk_app)
{
    App *app = g_new0(App, 1);
    app->list = wordlist_new();
    g_ptr_array_unref(app->list->words);
    app->list->words = g_ptr_array_new();
    app->all = wordlist_new();
    app->excluded_source = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    app->excluded_target = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    app->progress = progress_new();
    app->settings = settings_new();

    app->window = GTK_WINDOW(gtk_application_window_new(gtk_app));
    gtk_window_set_title(app->window, "yasvt-gui");
    gtk_window_set_default_size(app->window, 640, 360);
    g_object_set_data_full(G_OBJECT(app->window), "app", app, app_free);

    GtkWidget *open_button = gtk_button_new_with_label("Open…");
    g_signal_connect(open_button, "clicked", G_CALLBACK(on_open_clicked), app);
    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header), open_button);
    GtkWidget *settings_button = gtk_button_new_with_label("Settings");
    g_signal_connect(settings_button, "clicked", G_CALLBACK(on_settings_clicked), app);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), settings_button);
    gtk_window_set_titlebar(app->window, header);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_top(box, 24);
    gtk_widget_set_margin_bottom(box, 24);
    gtk_widget_set_margin_start(box, 24);
    gtk_widget_set_margin_end(box, 24);

    GtkWidget *modes = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_valign(modes, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(modes), gtk_label_new("Test modes"));
    for (int i = 0; i < MODE_COUNT; i++) {
        app->mode_check[i] = gtk_check_button_new_with_label(mode_labels[i]);
        gtk_check_button_set_active(GTK_CHECK_BUTTON(app->mode_check[i]), TRUE);
        gtk_box_append(GTK_BOX(modes), app->mode_check[i]);
    }

    app->prompt_label = gtk_label_new(NULL);

    app->word_label = gtk_label_new(NULL);
    gtk_widget_set_vexpand(app->word_label, TRUE);
    gtk_label_set_wrap(GTK_LABEL(app->word_label), TRUE);
    PangoAttrList *attrs = pango_attr_list_new();
    pango_attr_list_insert(attrs, pango_attr_scale_new(4.0));
    pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    gtk_label_set_attributes(GTK_LABEL(app->word_label), attrs);
    pango_attr_list_unref(attrs);

    app->entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry), "Type the translation and press Enter");
    g_signal_connect(app->entry, "activate", G_CALLBACK(on_answer), app);

    app->feedback_label = gtk_label_new(NULL);
    gtk_label_set_wrap(GTK_LABEL(app->feedback_label), TRUE);
    app->score_label = gtk_label_new(NULL);

    GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_halign(buttons, GTK_ALIGN_CENTER);
    add_button(buttons, "Speak", G_CALLBACK(on_speak_clicked), app);
    add_button(buttons, "Reverse", G_CALLBACK(on_reverse_clicked), app);
    add_button(buttons, "Shuffle", G_CALLBACK(on_shuffle_clicked), app);
    add_button(buttons, "Print list", G_CALLBACK(on_print_clicked), app);
    add_button(buttons, "Clear score", G_CALLBACK(on_clear_clicked), app);
    add_button(buttons, "Delete word", G_CALLBACK(on_delete_clicked), app);

    gtk_box_append(GTK_BOX(box), app->prompt_label);
    gtk_box_append(GTK_BOX(box), app->word_label);
    gtk_box_append(GTK_BOX(box), app->entry);
    gtk_box_append(GTK_BOX(box), app->feedback_label);
    gtk_box_append(GTK_BOX(box), app->score_label);
    gtk_box_append(GTK_BOX(box), buttons);
    gtk_widget_set_hexpand(box, TRUE);

    GtkWidget *filters = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_top(filters, 24);
    gtk_widget_set_margin_bottom(filters, 24);
    gtk_widget_set_margin_start(filters, 18);
    gtk_box_append(GTK_BOX(filters), gtk_label_new("Shown language"));
    app->source_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_append(GTK_BOX(filters), app->source_box);
    gtk_box_append(GTK_BOX(filters), gtk_label_new("Typed language"));
    app->target_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_box_append(GTK_BOX(filters), app->target_box);

    GtkWidget *filters_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(filters_scroll), GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_propagate_natural_width(GTK_SCROLLED_WINDOW(filters_scroll), TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(filters_scroll), filters);

    GtkWidget *layout = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_end(layout, 24);
    gtk_box_append(GTK_BOX(layout), filters_scroll);
    gtk_box_append(GTK_BOX(layout), box);
    gtk_box_append(GTK_BOX(layout), modes);
    gtk_window_set_child(app->window, layout);

    show_score(app);
    show_current_word(app);
    gtk_window_present(app->window);
    return app;
}

static void on_activate(GApplication *gtk_app, gpointer user_data)
{
    (void)user_data;
    App *app = build_window(GTK_APPLICATION(gtk_app));

    if (progress_load_words(app->progress, app->all) == 0)
        return;
    rebuild_filter_panel(app);
    char *message = g_strdup_printf("Loaded %u words from your progress. Open a file to use a different set.",
                                    app->all->words->len);
    gtk_label_set_text(GTK_LABEL(app->feedback_label), message);
    g_free(message);
    apply_filter(app);
}

static void on_open(GApplication *gtk_app, GFile **files, gint n_files, const char *hint,
                    gpointer user_data)
{
    (void)hint;
    (void)user_data;
    App *app = build_window(GTK_APPLICATION(gtk_app));

    char **paths = g_new0(char *, (gsize)n_files + 1);
    for (gint i = 0; i < n_files; i++)
        paths[i] = g_file_get_path(files[i]);
    load_paths(app, (const char *const *)paths, (guint)n_files);
    g_strfreev(paths);
}

static gint on_local_options(GApplication *gtk_app, GVariantDict *options, gpointer user_data)
{
    (void)gtk_app;
    (void)user_data;
    g_variant_dict_lookup(options, "start", "i", &range_start);
    g_variant_dict_lookup(options, "number", "i", &range_number);
    g_variant_dict_lookup(options, "new", "i", &new_percent);

    if (new_percent < 0 || new_percent > 100) {
        g_printerr("--new must be between 0 and 100\n");
        return 1;
    }
    if (range_start < 1) {
        g_printerr("--start must be at least 1\n");
        return 1;
    }
    return -1;
}

int gui_run(int argc, char **argv)
{
    GtkApplication *app = gtk_application_new("org.yasvt.gui", G_APPLICATION_HANDLES_OPEN);

    const GOptionEntry entries[] = {
        {"start", 's', 0, G_OPTION_ARG_INT, NULL, "First word to use, counted from 1 (default 1)", "N"},
        {"number", 'n', 0, G_OPTION_ARG_INT, NULL, "Number of words to use (default: all)", "N"},
        {"new", 'N', 0, G_OPTION_ARG_INT, NULL, "Percentage of new words in the initial order (default 20)", "PCT"},
        {NULL, 0, 0, 0, NULL, NULL, NULL},
    };
    g_application_add_main_option_entries(G_APPLICATION(app), entries);
    g_signal_connect(app, "handle-local-options", G_CALLBACK(on_local_options), NULL);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    g_signal_connect(app, "open", G_CALLBACK(on_open), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
