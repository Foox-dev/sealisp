#pragma once

#include <stddef.h>

typedef struct value value_t;
typedef struct env env_t;

typedef enum {
	VAL_NIL,
	VAL_BOOL,
	VAL_NUM,
	VAL_SYM,
	VAL_STR,
	VAL_CONS,
	VAL_BUILTIN,
	VAL_LAMBDA,
	VAL_ERR,
} value_type_t;

typedef value_t *(*builtin_t)(value_t *args, env_t *env);

typedef struct {
	value_t *car;
	value_t *cdr;
} cons_t;

typedef struct {
	value_t *params;
	value_t *body;
	env_t *env;
} lambda_t;

struct value {
	value_type_t type;
	union {
		double num;
		char *str;
		cons_t cons;
		builtin_t builtin;
		lambda_t lambda;
		int boolean;
	} as;
};

value_t *value_nil();
value_t *value_bool(int b);
value_t *value_num(double n);
value_t *value_sym(const char *s);
value_t *value_str(const char *s);
value_t *value_cons(value_t *car, value_t *cdr);
value_t *value_builtin(builtin_t fn);
value_t *value_lambda(value_t *params, value_t *body, env_t *env);
value_t *value_errf(const char *fmt, ...);

int value_is_nil(const value_t *v);
int value_is_truthy(const value_t *v);
int value_is_list(const value_t *v);

value_t *value_car(const value_t *v);
value_t *value_cdr(const value_t *v);
int value_list_length(const value_t *v);
