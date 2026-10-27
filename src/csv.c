#include "csv.h"

static void end_field(GPtrArray *fields, GString *field)
{
    g_ptr_array_add(fields, g_strdup(field->str));
    g_string_truncate(field, 0);
}

static void end_row(GPtrArray *rows, GPtrArray **fields, GString *field)
{
    end_field(*fields, field);
    g_ptr_array_add(*fields, NULL);
    g_ptr_array_add(rows, g_ptr_array_free(*fields, FALSE));
    *fields = g_ptr_array_new();
}

GPtrArray *csv_parse(const char *data, char delimiter)
{
    GPtrArray *rows = g_ptr_array_new_with_free_func((GDestroyNotify)g_strfreev);
    GPtrArray *fields = g_ptr_array_new();
    GString *field = g_string_new(NULL);
    gboolean in_quotes = FALSE;

    for (const char *p = data; *p != '\0'; p++) {
        if (in_quotes) {
            if (*p == '"') {
                if (p[1] == '"') {
                    g_string_append_c(field, '"');
                    p++;
                } else {
                    in_quotes = FALSE;
                }
            } else {
                g_string_append_c(field, *p);
            }
        } else if (*p == '"' && delimiter != '\t') {
            in_quotes = TRUE;
        } else if (*p == delimiter) {
            end_field(fields, field);
        } else if (*p == '\n') {
            end_row(rows, &fields, field);
        } else if (*p != '\r') {
            g_string_append_c(field, *p);
        }
    }

    if (field->len > 0 || fields->len > 0)
        end_row(rows, &fields, field);

    g_ptr_array_free(fields, TRUE);
    g_string_free(field, TRUE);
    return rows;
}
