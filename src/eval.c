#include <string.h>

#include "eval.h"

#define MIN_ARGS_IF 2
#define MAX_ARGS_IF 3

static int is_error(const value_t *v);
static value_t *eval_list(value_t *list, env_t *env);
static value_t *eval_body(value_t *body, env_t *env);
static value_t *sf_quote(value_t *args);
static value_t *sf_if(value_t *args, env_t *env);
static value_t *sf_define(value_t *args, env_t *env);
static value_t *sf_lambda(value_t *args, env_t *env);
static value_t *sf_begin(value_t *args, env_t *env);
static value_t *eval_cons(value_t *expr, env_t *env);

static int is_error(const value_t *v) {
	return v->type == VAL_ERR;
}

static value_t *eval_list(value_t *list, env_t *env) {
	value_t *item;
	value_t *tail;

	if (value_is_nil(list)) {
		return value_nil();
	}

	item = eval(value_car(list), env);
	if (is_error(item)) {
		return item;
	}

	tail = eval_list(value_cdr(list), env);
	if (is_error(tail)) {
		return tail;
	}

	return value_cons(item, tail);
}

static value_t *eval_body(value_t *body, env_t *env) {
	value_t *result;

	result = value_nil();
	while (!value_is_nil(body)) {
		result = eval(value_car(body), env);
		if (is_error(result)) {
			return result;
		}
		body = value_cdr(body);
	}
	return result;
}

static value_t *sf_quote(value_t *args) {
	if (value_is_nil(args)) {
		return value_errf("quote: missing argument");
	}
	return value_car(args);
}

static value_t *sf_if(value_t *args, env_t *env) {
	value_t *cond;
	value_t *branch;

	if (value_list_length(args) < MIN_ARGS_IF || value_list_length(args) > MAX_ARGS_IF) {
		return value_errf("if: expects 2 or 3 arguments");
	}

	cond = eval(value_car(args), env);
	if (is_error(cond)) {
		return cond;
	}

	if (value_is_truthy(cond)) {
		return eval(value_car(value_cdr(args)), env);
	}

	branch = value_cdr(value_cdr(args));
	if (value_is_nil(branch)) {
		return value_nil();
	}
	return eval(value_car(branch), env);
}

static value_t *sf_define(value_t *args, env_t *env) {
	value_t *target;
	value_t *name;
	value_t *val;

	if (value_is_nil(args)) {
		return value_errf("define: missing name");
	}

	target = value_car(args);
	if (target->type == VAL_CONS) {
		name = value_car(target);
		val = value_lambda(value_cdr(target), value_cdr(args), env);
		env_define(env, name->as.str, val);
		return name;
	}

	name = target;
	if (value_is_nil(value_cdr(args))) {
		return value_errf("define: missing value for %s", name->as.str);
	}

	val = eval(value_car(value_cdr(args)), env);
	if (is_error(val)) {
		return val;
	}

	env_define(env, name->as.str, val);
	return name;
}

static value_t *sf_lambda(value_t *args, env_t *env) {
	value_t *params;

	if (value_is_nil(args)) {
		return value_errf("lambda: missing parameter list");
	}

	params = value_car(args);
	if (params->type != VAL_NIL && params->type != VAL_CONS) {
		return value_errf("lambda: expects a parameter list");
	}

	return value_lambda(params, value_cdr(args), env);
}

static value_t *sf_begin(value_t *args, env_t *env) {
	return eval_body(args, env);
}

static value_t *eval_cons(value_t *expr, env_t *env) {
	value_t *head;
	value_t *args;
	value_t *fn;
	value_t *evaled_args;

	head = value_car(expr);
	args = value_cdr(expr);

	if (head->type == VAL_SYM) {
		if (strcmp(head->as.str, "quote") == 0) {
			return sf_quote(args);
		}
		if (strcmp(head->as.str, "if") == 0) {
			return sf_if(args, env);
		}
		if (strcmp(head->as.str, "define") == 0) {
			return sf_define(args, env);
		}
		if (strcmp(head->as.str, "lambda") == 0) {
			return sf_lambda(args, env);
		}
		if (strcmp(head->as.str, "begin") == 0) {
			return sf_begin(args, env);
		}
	}

	fn = eval(head, env);
	if (is_error(fn)) {
		return fn;
	}

	evaled_args = eval_list(args, env);
	if (is_error(evaled_args)) {
		return evaled_args;
	}

	return apply(fn, evaled_args, env);
}

value_t *eval(value_t *expr, env_t *env) {
	value_t *val;

	switch (expr->type) {
	case VAL_SYM:
		val = env_get(env, expr->as.str);
		if (!val) {
			return value_errf("unbound symbol: %s", expr->as.str);
		}
		return val;
	case VAL_CONS:
		return eval_cons(expr, env);
	default:
		return expr;
	}
}

value_t *apply(value_t *fn, value_t *args, env_t *env) {
	env_t *call_env;
	value_t *params;
	value_t *bound_args;

	if (fn->type == VAL_BUILTIN) {
		return fn->as.builtin(args, env);
	}

	if (fn->type != VAL_LAMBDA) {
		return value_errf("not a function");
	}

	call_env = env_new(fn->as.lambda.env);
	params = fn->as.lambda.params;
	bound_args = args;
	while (!value_is_nil(params)) {
		if (value_is_nil(bound_args)) {
			return value_errf("too few arguments");
		}
		env_define(call_env, value_car(params)->as.str, value_car(bound_args));
		params = value_cdr(params);
		bound_args = value_cdr(bound_args);
	}
	if (!value_is_nil(bound_args)) {
		return value_errf("too many arguments");
	}

	return eval_body(fn->as.lambda.body, call_env);
}
