#include <glib.h>

#include "hanzi.h"
#include "hanzi_table.h"

static gunichar lookup(const HanziEntry *entries, unsigned count, gunichar ch)
{
    unsigned low = 0, high = count;

    while (low < high) {
        unsigned mid = low + (high - low) / 2;
        if (entries[mid].from < ch)
            low = mid + 1;
        else
            high = mid;
    }
    return low < count && entries[low].from == ch ? entries[low].to : ch;
}

static char *convert(const char *text, const HanziEntry *entries, unsigned count)
{
    GString *out = g_string_new(NULL);
    for (const char *p = text; *p != '\0'; p = g_utf8_next_char(p))
        g_string_append_unichar(out, lookup(entries, count, g_utf8_get_char(p)));
    return g_string_free(out, FALSE);
}

char *hanzi_to_simplified(const char *text)
{
    return convert(text, hanzi_t2s, hanzi_t2s_count);
}

char *hanzi_to_traditional(const char *text)
{
    return convert(text, hanzi_s2t, hanzi_s2t_count);
}
