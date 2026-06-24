#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "builtins.h"
#include "printer.h"

typedef int (*num_cmp_t)(double a, double b);

static void env_define_builtin(env_t *env, const char *name, builtin_t fn);
static int values_equal(const value_t *a, const value_t *b);
static int cmp_eq(double a, double b);
static int cmp_lt(double a, double b);
static int cmp_gt(double a, double b);
static int cmp_le(double a, double b);
static int cmp_ge(double a, double b);
static value_t *chain_compare(value_t *args, num_cmp_t cmp, const char *name);
static value_t *bi_add(value_t *args, env_t *env);
static value_t *bi_sub(value_t *args, env_t *env);
static value_t *bi_mul(value_t *args, env_t *env);
static value_t *bi_div(value_t *args, env_t *env);
static value_t *bi_eq_num(value_t *args, env_t *env);
static value_t *bi_lt(value_t *args, env_t *env);
static value_t *bi_gt(value_t *args, env_t *env);
static value_t *bi_le(value_t *args, env_t *env);
static value_t *bi_ge(value_t *args, env_t *env);
static value_t *bi_cons(value_t *args, env_t *env);
static value_t *bi_car(value_t *args, env_t *env);
static value_t *bi_cdr(value_t *args, env_t *env);
static value_t *bi_list(value_t *args, env_t *env);
static value_t *bi_is_null(value_t *args, env_t *env);
static value_t *bi_is_pair(value_t *args, env_t *env);
static value_t *bi_equal(value_t *args, env_t *env);
static value_t *bi_not(value_t *args, env_t *env);
static value_t *bi_display(value_t *args, env_t *env);
static value_t *bi_newline(value_t *args, env_t *env);
static value_t *bi_exit(value_t *args, env_t *env);

static void env_define_builtin(env_t *env, const char *name, builtin_t fn) {
	env_define(env, name, value_builtin(fn));
}

static int values_equal(const value_t *a, const value_t *b) {
	if (a->type != b->type) {
		return 0;
	}
	switch (a->type) {
	case VAL_NIL:
		return 1;
	case VAL_BOOL:
		return a->as.boolean == b->as.boolean;
	case VAL_NUM:
		return a->as.num == b->as.num;
	case VAL_SYM:
	case VAL_STR:
	case VAL_ERR:
		return strcmp(a->as.str, b->as.str) == 0;
	case VAL_CONS:
		return values_equal(a->as.cons.car, b->as.cons.car) && values_equal(a->as.cons.cdr, b->as.cons.cdr);
	case VAL_BUILTIN:
		return a->as.builtin == b->as.builtin;
	case VAL_LAMBDA:
		return a == b;
	}
	return 0;
}

static int cmp_eq(double a, double b) {
	return a == b;
}

static int cmp_lt(double a, double b) {
	return a < b;
}

static int cmp_gt(double a, double b) {
	return a > b;
}

static int cmp_le(double a, double b) {
	return a <= b;
}

static int cmp_ge(double a, double b) {
	return a >= b;
}

static value_t *chain_compare(value_t *args, num_cmp_t cmp, const char *name) {
	value_t *a;
	value_t *b;

	if (value_is_nil(args) || value_is_nil(value_cdr(args))) {
		return value_errf("%s: expects at least 2 arguments", name);
	}

	while (!value_is_nil(value_cdr(args))) {
		a = value_car(args);
		b = value_car(value_cdr(args));
		if (a->type != VAL_NUM || b->type != VAL_NUM) {
			return value_errf("%s: expects numbers", name);
		}
		if (!cmp(a->as.num, b->as.num)) {
			return value_bool(0);
		}
		args = value_cdr(args);
	}
	return value_bool(1);
}

static value_t *bi_add(value_t *args, env_t *env) {
	double total;
	value_t *v;

	(void)env;

	total = 0;
	while (!value_is_nil(args)) {
		v = value_car(args);
		if (v->type != VAL_NUM) {
			return value_errf("+: expects numbers");
		}
		total += v->as.num;
		args = value_cdr(args);
	}
	return value_num(total);
}

static value_t *bi_sub(value_t *args, env_t *env) {
	double total;
	value_t *v;

	(void)env;

	if (value_is_nil(args)) {
		return value_errf("-: expects at least 1 argument");
	}

	v = value_car(args);
	if (v->type != VAL_NUM) {
		return value_errf("-: expects numbers");
	}

	if (value_is_nil(value_cdr(args))) {
		return value_num(-v->as.num);
	}

	total = v->as.num;
	args = value_cdr(args);
	while (!value_is_nil(args)) {
		v = value_car(args);
		if (v->type != VAL_NUM) {
			return value_errf("-: expects numbers");
		}
		total -= v->as.num;
		args = value_cdr(args);
	}
	return value_num(total);
}

static value_t *bi_mul(value_t *args, env_t *env) {
	double total;
	value_t *v;

	(void)env;

	total = 1;
	while (!value_is_nil(args)) {
		v = value_car(args);
		if (v->type != VAL_NUM) {
			return value_errf("*: expects numbers");
		}
		total *= v->as.num;
		args = value_cdr(args);
	}
	return value_num(total);
}

static value_t *bi_div(value_t *args, env_t *env) {
	double total;
	value_t *v;

	(void)env;

	if (value_is_nil(args)) {
		return value_errf("/: expects at least 1 argument");
	}

	v = value_car(args);
	if (v->type != VAL_NUM) {
		return value_errf("/: expects numbers");
	}

	if (value_is_nil(value_cdr(args))) {
		if (v->as.num == 0) {
			return value_errf("/: division by zero");
		}
		return value_num(1 / v->as.num);
	}

	total = v->as.num;
	args = value_cdr(args);
	while (!value_is_nil(args)) {
		v = value_car(args);
		if (v->type != VAL_NUM) {
			return value_errf("/: expects numbers");
		}
		if (v->as.num == 0) {
			return value_errf("/: division by zero");
		}
		total /= v->as.num;
		args = value_cdr(args);
	}
	return value_num(total);
}

static value_t *bi_eq_num(value_t *args, env_t *env) {
	(void)env;
	return chain_compare(args, cmp_eq, "=");
}

static value_t *bi_lt(value_t *args, env_t *env) {
	(void)env;
	return chain_compare(args, cmp_lt, "<");
}

static value_t *bi_gt(value_t *args, env_t *env) {
	(void)env;
	return chain_compare(args, cmp_gt, ">");
}

static value_t *bi_le(value_t *args, env_t *env) {
	(void)env;
	return chain_compare(args, cmp_le, "<=");
}

static value_t *bi_ge(value_t *args, env_t *env) {
	(void)env;
	return chain_compare(args, cmp_ge, ">=");
}

static value_t *bi_cons(value_t *args, env_t *env) {
	(void)env;

	if (value_list_length(args) != 2) {
		return value_errf("cons: expects 2 arguments");
	}
	return value_cons(value_car(args), value_car(value_cdr(args)));
}

static value_t *bi_car(value_t *args, env_t *env) {
	value_t *v;

	(void)env;

	if (value_list_length(args) != 1) {
		return value_errf("car: expects 1 argument");
	}
	v = value_car(args);
	if (v->type != VAL_CONS) {
		return value_errf("car: expects a pair");
	}
	return value_car(v);
}

static value_t *bi_cdr(value_t *args, env_t *env) {
	value_t *v;

	(void)env;

	if (value_list_length(args) != 1) {
		return value_errf("cdr: expects 1 argument");
	}
	v = value_car(args);
	if (v->type != VAL_CONS) {
		return value_errf("cdr: expects a pair");
	}
	return value_cdr(v);
}

static value_t *bi_list(value_t *args, env_t *env) {
	(void)env;
	return args;
}

static value_t *bi_is_null(value_t *args, env_t *env) {
	(void)env;

	if (value_list_length(args) != 1) {
		return value_errf("null?: expects 1 argument");
	}
	return value_bool(value_is_nil(value_car(args)));
}

static value_t *bi_is_pair(value_t *args, env_t *env) {
	(void)env;

	if (value_list_length(args) != 1) {
		return value_errf("pair?: expects 1 argument");
	}
	return value_bool(value_car(args)->type == VAL_CONS);
}

static value_t *bi_equal(value_t *args, env_t *env) {
	(void)env;

	if (value_list_length(args) != 2) {
		return value_errf("equal?: expects 2 arguments");
	}
	return value_bool(values_equal(value_car(args), value_car(value_cdr(args))));
}

static value_t *bi_not(value_t *args, env_t *env) {
	(void)env;

	if (value_list_length(args) != 1) {
		return value_errf("not: expects 1 argument");
	}
	return value_bool(!value_is_truthy(value_car(args)));
}

static value_t *bi_display(value_t *args, env_t *env) {
	(void)env;

	if (value_list_length(args) != 1) {
		return value_errf("display: expects 1 argument");
	}
	value_print(stdout, value_car(args));
	return value_nil();
}

static value_t *bi_newline(value_t *args, env_t *env) {
	(void)env;
	(void)args;

	fputc('\n', stdout);
	return value_nil();
}

static value_t *bi_exit(value_t *args, env_t *env) {
	(void)env;
	(void)args;

	exit(0);
}

void builtins_install(env_t *env) {
	env_define_builtin(env, "+", bi_add);
	env_define_builtin(env, "-", bi_sub);
	env_define_builtin(env, "*", bi_mul);
	env_define_builtin(env, "/", bi_div);
	env_define_builtin(env, "=", bi_eq_num);
	env_define_builtin(env, "<", bi_lt);
	env_define_builtin(env, ">", bi_gt);
	env_define_builtin(env, "<=", bi_le);
	env_define_builtin(env, ">=", bi_ge);
	env_define_builtin(env, "cons", bi_cons);
	env_define_builtin(env, "car", bi_car);
	env_define_builtin(env, "cdr", bi_cdr);
	env_define_builtin(env, "list", bi_list);
	env_define_builtin(env, "null?", bi_is_null);
	env_define_builtin(env, "pair?", bi_is_pair);
	env_define_builtin(env, "equal?", bi_equal);
	env_define_builtin(env, "not", bi_not);
	env_define_builtin(env, "display", bi_display);
	env_define_builtin(env, "newline", bi_newline);
	env_define_builtin(env, "exit", bi_exit);
}
