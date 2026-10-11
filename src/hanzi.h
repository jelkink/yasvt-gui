#ifndef HANZI_H
#define HANZI_H

/* Character-by-character conversion; characters without a mapping are kept. */
char *hanzi_to_simplified(const char *text);
char *hanzi_to_traditional(const char *text);

#endif
