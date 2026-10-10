#ifndef PINYIN_TABLE_H
#define PINYIN_TABLE_H

#include <stdint.h>

typedef struct {
    uint32_t codepoint;
    uint16_t syllable; /* index into pinyin_syllables */
} PinyinEntry;

extern const char *const pinyin_syllables[];
extern const PinyinEntry pinyin_entries[]; /* sorted by codepoint */
extern const unsigned pinyin_entry_count;

#endif
