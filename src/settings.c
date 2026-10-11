#include "settings.h"

struct Settings {
    GHashTable *native; /* language name set */
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
    settings->path = g_build_filename(g_get_user_config_dir(), "yasvt-gui", "settings.ini", NULL);

    GKeyFile *file = g_key_file_new();
    gsize length = 0;
    char **names = NULL;
    if (g_key_file_load_from_file(file, settings->path, G_KEY_FILE_NONE, NULL))
        names = g_key_file_get_string_list(file, "general", "native_languages", &length, NULL);

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
    g_free(settings->path);
    g_free(settings);
}

gboolean settings_is_native(const Settings *settings, const char *language)
{
    return g_hash_table_contains(settings->native, language);
}

void settings_set_native(Settings *settings, const char *language, gboolean native)
{
    if (native)
        g_hash_table_add(settings->native, g_strdup(language));
    else
        g_hash_table_remove(settings->native, language);
    save(settings);
}
