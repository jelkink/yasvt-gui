#ifndef CSV_H
#define CSV_H

#include <glib.h>

/* Parses CSV text (RFC 4180 quoting). Returns a GPtrArray of NULL-terminated
 * string vectors (gchar **), one per line; free with g_ptr_array_unref(). */
GPtrArray *csv_parse(const char *data, char delimiter);

#endif
