#include <stdio.h>

#include "printer.h"

static void print_cons(FILE *out, const value_t *v);
static void print_num(FILE *out, double n);

static void print_num(FILE *out, double n) {
	if (n == (long)n) {
		fprintf(out, "%ld", (long)n);
	} else {
		fprintf(out, "%g", n);
	}
}

static void print_cons(FILE *out, const value_t *v) {
	const value_t *rest;

	fputc('(', out);
	rest = v;
	while (rest->type == VAL_CONS) {
		value_print(out, rest->as.cons.car);
		rest = rest->as.cons.cdr;
		if (rest->type == VAL_CONS) {
			fputc(' ', out);
		}
	}
	if (rest->type != VAL_NIL) {
		fputs(" . ", out);
		value_print(out, rest);
	}
	fputc(')', out);
}

void value_print(FILE *out, const value_t *v) {
	switch (v->type) {
	case VAL_NIL:
		fputs("()", out);
		break;
	case VAL_BOOL:
		fputs(v->as.boolean ? "#t" : "#f", out);
		break;
	case VAL_NUM:
		print_num(out, v->as.num);
		break;
	case VAL_SYM:
		fputs(v->as.str, out);
		break;
	case VAL_STR:
		fprintf(out, "\"%s\"", v->as.str);
		break;
	case VAL_CONS:
		print_cons(out, v);
		break;
	case VAL_BUILTIN:
		fputs("#<builtin>", out);
		break;
	case VAL_LAMBDA:
		fputs("#<lambda>", out);
		break;
	case VAL_ERR:
		fprintf(out, "error: %s", v->as.str);
		break;
	}
}
