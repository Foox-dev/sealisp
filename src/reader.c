#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "reader.h"

#define TOKEN_MAX 256
#define LIST_INITIAL_CAP 8

static int reader_peek(reader_t *r);
static int reader_advance(reader_t *r);
static void reader_skip_ws(reader_t *r);
static int is_delim(int c);
static value_t *read_form(reader_t *r);
static value_t *read_list(reader_t *r);
static value_t *read_string(reader_t *r);
static value_t *read_atom(reader_t *r);
static value_t *atom_to_value(const char *tok);

static int reader_peek(reader_t *r) {
	if (r->pos >= r->len) {
		return -1;
	}
	return (unsigned char)r->src[r->pos];
}

static int reader_advance(reader_t *r) {
	int c;

	c = reader_peek(r);
	if (c != -1) {
		r->pos++;
	}
	return c;
}

static void reader_skip_ws(reader_t *r) {
	int c;

	for (;;) {
		c = reader_peek(r);
		if (c == ';') {
			while (c != -1 && c != '\n') {
				reader_advance(r);
				c = reader_peek(r);
			}
			continue;
		}
		if (c == -1 || !isspace((unsigned char)c)) {
			return;
		}
		reader_advance(r);
	}
}

static int is_delim(int c) {
	return c == -1 || isspace((unsigned char)c) || c == '(' || c == ')' || c == '"' || c == ';' || c == '\'';
}

static value_t *read_form(reader_t *r) {
	int c;
	value_t *quoted;

	reader_skip_ws(r);
	c = reader_peek(r);
	if (c == -1) {
		return NULL;
	}
	if (c == '(') {
		reader_advance(r);
		return read_list(r);
	}
	if (c == ')') {
		reader_advance(r);
		return value_errf("unexpected )");
	}
	if (c == '\'') {
		reader_advance(r);
		quoted = read_form(r);
		if (!quoted) {
			return value_errf("unexpected EOF after '");
		}
		return value_cons(value_sym("quote"), value_cons(quoted, value_nil()));
	}
	if (c == '"') {
		return read_string(r);
	}
	return read_atom(r);
}

static value_t *read_list(reader_t *r) {
	value_t **items;
	size_t cap;
	size_t count;
	value_t *result;
	value_t *item;
	int c;
	size_t i;

	cap = LIST_INITIAL_CAP;
	count = 0;
	items = malloc(cap * sizeof(value_t *));

	for (;;) {
		reader_skip_ws(r);
		c = reader_peek(r);
		if (c == -1) {
			free(items);
			return value_errf("unexpected EOF, expected )");
		}
		if (c == ')') {
			reader_advance(r);
			break;
		}
		item = read_form(r);
		if (item->type == VAL_ERR) {
			free(items);
			return item;
		}
		if (count == cap) {
			cap *= 2;
			items = realloc(items, cap * sizeof(value_t *));
		}
		items[count++] = item;
	}

	result = value_nil();
	for (i = count; i > 0; i--) {
		result = value_cons(items[i - 1], result);
	}
	free(items);
	return result;
}

static value_t *read_string(reader_t *r) {
	char buf[TOKEN_MAX];
	size_t n;
	int c;

	reader_advance(r);
	n = 0;
	for (;;) {
		c = reader_advance(r);
		if (c == -1) {
			return value_errf("unterminated string literal");
		}
		if (c == '"') {
			break;
		}
		if (c == '\\') {
			c = reader_advance(r);
			if (c == 'n') {
				c = '\n';
			} else if (c == 't') {
				c = '\t';
			}
		}
		if (n < sizeof(buf) - 1) {
			buf[n++] = (char)c;
		}
	}
	buf[n] = '\0';
	return value_str(buf);
}

static value_t *read_atom(reader_t *r) {
	char buf[TOKEN_MAX];
	size_t n;
	int c;

	n = 0;
	for (;;) {
		c = reader_peek(r);
		if (is_delim(c)) {
			break;
		}
		if (n < sizeof(buf) - 1) {
			buf[n++] = (char)c;
		}
		reader_advance(r);
	}
	buf[n] = '\0';
	return atom_to_value(buf);
}

static value_t *atom_to_value(const char *tok) {
	char *end;
	double n;

	if (strcmp(tok, "#t") == 0) {
		return value_bool(1);
	}
	if (strcmp(tok, "#f") == 0) {
		return value_bool(0);
	}
	n = strtod(tok, &end);
	if (end != tok && *end == '\0') {
		return value_num(n);
	}
	return value_sym(tok);
}

reader_t reader_new(const char *src) {
	return (reader_t) {
		.src = src,
		.pos = 0,
		.len = strlen(src),
	};
}

value_t *reader_read(reader_t *r) {
	return read_form(r);
}

int reader_at_eof(reader_t *r) {
	reader_skip_ws(r);
	return reader_peek(r) == -1;
}

int reader_is_complete(const char *src) {
	int depth;
	int in_string;
	int in_comment;
	size_t i;
	char c;

	depth = 0;
	in_string = 0;
	in_comment = 0;
	for (i = 0; src[i] != '\0'; i++) {
		c = src[i];
		if (in_comment) {
			if (c == '\n') {
				in_comment = 0;
			}
			continue;
		}
		if (in_string) {
			if (c == '\\') {
				i++;
			} else if (c == '"') {
				in_string = 0;
			}
			continue;
		}
		if (c == ';') {
			in_comment = 1;
		} else if (c == '"') {
			in_string = 1;
		} else if (c == '(') {
			depth++;
		} else if (c == ')') {
			depth--;
		}
	}
	return depth <= 0 && !in_string;
}
