#ifndef PINYIN_H
#define PINYIN_H

/* Pinyin with tone marks for the Chinese characters in text, separated by spaces.
 * Other characters are kept as they are. Returns NULL if text has no Chinese
 * characters. Each character uses its most common reading. */
char *pinyin_from_text(const char *text);

#endif
