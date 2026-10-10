#include "pinyin.h"

#include <glib.h>

#include "pinyin_table.h"

static const char *lookup(gunichar ch)
{
    unsigned low = 0, high = pinyin_entry_count;

    while (low < high) {
        unsigned mid = low + (high - low) / 2;
        if (pinyin_entries[mid].codepoint < ch)
            low = mid + 1;
        else
            high = mid;
    }

    if (low < pinyin_entry_count && pinyin_entries[low].codepoint == ch)
        return pinyin_syllables[pinyin_entries[low].syllable];
    return NULL;
}

char *pinyin_from_text(const char *text)
{
    GString *out = g_string_new(NULL);
    gboolean found = FALSE;
    gboolean in_other = FALSE;

    for (const char *p = text; *p != '\0'; p = g_utf8_next_char(p)) {
        gunichar ch = g_utf8_get_char(p);
        const char *syllable = lookup(ch);

        if (syllable != NULL) {
            if (out->len > 0 && out->str[out->len - 1] != ' ')
                g_string_append_c(out, ' ');
            g_string_append(out, syllable);
            found = TRUE;
            in_other = FALSE;
        } else {
            if (!in_other && out->len > 0 && ch != ' ')
                g_string_append_c(out, ' ');
            g_string_append_unichar(out, ch);
            in_other = ch != ' ';
        }
    }

    return g_string_free(out, !found);
}
