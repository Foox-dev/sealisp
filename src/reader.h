#pragma once

#include <stddef.h>

#include "value.h"

typedef struct {
	const char *src;
	size_t pos;
	size_t len;
} reader_t;

reader_t reader_new(const char *src);
value_t *reader_read(reader_t *r);
int reader_at_eof(reader_t *r);
int reader_is_complete(const char *src);
