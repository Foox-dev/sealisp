#pragma once

#include "value.h"

typedef struct env_entry env_entry_t;

struct env {
	env_entry_t *entries;
	env_t *parent;
};

env_t *env_new(env_t *parent);
void env_define(env_t *env, const char *name, value_t *val);
int env_set(env_t *env, const char *name, value_t *val);
value_t *env_get(env_t *env, const char *name);
