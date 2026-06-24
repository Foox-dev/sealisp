#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "value.h"

#define ERR_MSG_MAX 256

static value_t *value_alloc(value_type_t type);

static value_t *value_alloc(value_type_t type) {
	value_t *v;

	v = malloc(sizeof(value_t));
	v->type = type;
	return v;
}

value_t *value_nil() {
	return value_alloc(VAL_NIL);
}

value_t *value_bool(int b) {
	value_t *v;

	v = value_alloc(VAL_BOOL);
	v->as.boolean = b;
	return v;
}

value_t *value_num(double n) {
	value_t *v;

	v = value_alloc(VAL_NUM);
	v->as.num = n;
	return v;
}

value_t *value_sym(const char *s) {
	value_t *v;

	v = value_alloc(VAL_SYM);
	v->as.str = strdup(s);
	return v;
}

value_t *value_str(const char *s) {
	value_t *v;

	v = value_alloc(VAL_STR);
	v->as.str = strdup(s);
	return v;
}

value_t *value_cons(value_t *car, value_t *cdr) {
	value_t *v;

	v = value_alloc(VAL_CONS);
	v->as.cons = (cons_t) {
		.car = car,
		.cdr = cdr,
	};
	return v;
}

value_t *value_builtin(builtin_t fn) {
	value_t *v;

	v = value_alloc(VAL_BUILTIN);
	v->as.builtin = fn;
	return v;
}

value_t *value_lambda(value_t *params, value_t *body, env_t *env) {
	value_t *v;

	v = value_alloc(VAL_LAMBDA);
	v->as.lambda = (lambda_t) {
		.params = params,
		.body = body,
		.env = env,
	};
	return v;
}

value_t *value_errf(const char *fmt, ...) {
	value_t *v;
	char msg[ERR_MSG_MAX];
	va_list args;

	va_start(args, fmt);
	vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

	v = value_alloc(VAL_ERR);
	v->as.str = strdup(msg);
	return v;
}

int value_is_nil(const value_t *v) {
	return v->type == VAL_NIL;
}

int value_is_truthy(const value_t *v) {
	return !(v->type == VAL_BOOL && !v->as.boolean);
}

int value_is_list(const value_t *v) {
	while (v->type == VAL_CONS) {
		v = v->as.cons.cdr;
	}
	return v->type == VAL_NIL;
}

value_t *value_car(const value_t *v) {
	return v->as.cons.car;
}

value_t *value_cdr(const value_t *v) {
	return v->as.cons.cdr;
}

int value_list_length(const value_t *v) {
	int n;

	n = 0;
	while (v->type == VAL_CONS) {
		n++;
		v = v->as.cons.cdr;
	}
	return n;
}
