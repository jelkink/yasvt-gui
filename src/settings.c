#include "settings.h"

struct Settings {
    GHashTable *native; /* language name set */
    GHashTable *files;  /* absolute path -> {target, source, "header" or "words"} */
    gboolean traditional;
    char *path;
};

static void save(const Settings *settings)
{
    GKeyFile *file = g_key_file_new();
    GHashTableIter iter;
    gpointer key;
    GPtrArray *names = g_ptr_array_new();

    g_hash_table_iter_init(&iter, settings->native);
    while (g_hash_table_iter_next(&iter, &key, NULL))
        g_ptr_array_add(names, key);

    g_key_file_set_string_list(file, "general", "native_languages",
                               (const char *const *)names->pdata, names->len);
    g_ptr_array_free(names, TRUE);
    g_key_file_set_boolean(file, "general", "traditional", settings->traditional);

    /* Keyed by a hash, since paths may contain characters key names cannot. */
    GHashTableIter files;
    gpointer path, columns;
    g_hash_table_iter_init(&files, settings->files);
    while (g_hash_table_iter_next(&files, &path, &columns)) {
        const char *const *c = columns;
        const char *list[] = {path, c[0], c[1], c[2]};
        char *key = g_compute_checksum_for_string(G_CHECKSUM_MD5, path, -1);
        g_key_file_set_string_list(file, "files", key, list, G_N_ELEMENTS(list));
        g_free(key);
    }

    char *dir = g_path_get_dirname(settings->path);
    g_mkdir_with_parents(dir, 0700);
    g_key_file_save_to_file(file, settings->path, NULL);
    g_free(dir);
    g_key_file_free(file);
}

Settings *settings_new(void)
{
    Settings *settings = g_new0(Settings, 1);
    settings->native = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    settings->files = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_strfreev);
    settings->path = g_build_filename(g_get_user_config_dir(), "yasvt-gui", "settings.ini", NULL);

    GKeyFile *file = g_key_file_new();
    gsize length = 0;
    char **names = NULL;
    if (g_key_file_load_from_file(file, settings->path, G_KEY_FILE_NONE, NULL)) {
        names = g_key_file_get_string_list(file, "general", "native_languages", &length, NULL);
        settings->traditional = g_key_file_get_boolean(file, "general", "traditional", NULL);

        char **keys = g_key_file_get_keys(file, "files", NULL, NULL);
        for (char **key = keys; key != NULL && *key != NULL; key++) {
            gsize count = 0;
            char **list = g_key_file_get_string_list(file, "files", *key, &count, NULL);
            if (count == 4) {
                char *columns[] = {list[1], list[2], list[3], NULL};
                g_hash_table_insert(settings->files, g_strdup(list[0]), g_strdupv(columns));
            }
            g_strfreev(list);
        }
        g_strfreev(keys);
    }

    if (names != NULL) {
        for (gsize i = 0; i < length; i++)
            g_hash_table_add(settings->native, g_strdup(names[i]));
    } else {
        g_hash_table_add(settings->native, g_strdup("english"));
        g_hash_table_add(settings->native, g_strdup("dutch"));
    }

    g_strfreev(names);
    g_key_file_free(file);
    return settings;
}

void settings_free(Settings *settings)
{
    if (settings == NULL)
        return;
    g_hash_table_unref(settings->native);
    g_hash_table_unref(settings->files);
    g_free(settings->path);
    g_free(settings);
}

gboolean settings_is_native(const Settings *settings, const char *language)
{
    return g_hash_table_contains(settings->native, language);
}

gboolean settings_get_traditional(const Settings *settings)
{
    return settings->traditional;
}

void settings_set_traditional(Settings *settings, gboolean traditional)
{
    settings->traditional = traditional;
    save(settings);
}

void settings_set_native(Settings *settings, const char *language, gboolean native)
{
    if (native)
        g_hash_table_add(settings->native, g_strdup(language));
    else
        g_hash_table_remove(settings->native, language);
    save(settings);
}

gboolean settings_get_file_languages(const Settings *settings, const char *path, char **target,
                                     char **source, gboolean *header)
{
    char *absolute = g_canonicalize_filename(path, NULL);
    const char *const *columns = g_hash_table_lookup(settings->files, absolute);
    g_free(absolute);
    if (columns == NULL)
        return FALSE;

    *target = g_strdup(columns[0]);
    *source = g_strdup(columns[1]);
    *header = g_strcmp0(columns[2], "header") == 0;
    return TRUE;
}

void settings_set_file_languages(Settings *settings, const char *path, const char *target,
                                 const char *source, gboolean header)
{
    char *columns[] = {(char *)target, (char *)source, header ? "header" : "words", NULL};
    g_hash_table_insert(settings->files, g_canonicalize_filename(path, NULL), g_strdupv(columns));
    save(settings);
}
