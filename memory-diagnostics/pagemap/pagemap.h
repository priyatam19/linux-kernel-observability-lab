#ifndef PAGEMAP_H
#define PAGEMAP_H
#include <stdio.h>
#include <stddef.h>
int dump_va_pa_with_latency(int fd, void *base, size_t size, size_t page_size, FILE *out);
#endif
