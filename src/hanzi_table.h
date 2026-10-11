#ifndef HANZI_TABLE_H
#define HANZI_TABLE_H

#include <stdint.h>

typedef struct {
    uint32_t from;
    uint32_t to;
} HanziEntry;

extern const HanziEntry hanzi_t2s[]; /* sorted by `from` */
extern const unsigned hanzi_t2s_count;
extern const HanziEntry hanzi_s2t[]; /* sorted by `from` */
extern const unsigned hanzi_s2t_count;

#endif
