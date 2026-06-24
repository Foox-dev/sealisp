#include <stdlib.h>
#include <string.h>

#include "env.h"

struct env_entry {
	char *name;
	value_t *val;
	env_entry_t *next;
};

static env_entry_t *env_entry_new(const char *name, value_t *val, env_entry_t *next);

static env_entry_t *env_entry_new(const char *name, value_t *val, env_entry_t *next) {
	env_entry_t *entry;

	entry = malloc(sizeof(env_entry_t));
	entry->name = strdup(name);
	entry->val = val;
	entry->next = next;
	return entry;
}

env_t *env_new(env_t *parent) {
	env_t *env;

	env = malloc(sizeof(env_t));
	env->entries = NULL;
	env->parent = parent;
	return env;
}

void env_define(env_t *env, const char *name, value_t *val) {
	env->entries = env_entry_new(name, val, env->entries);
}

int env_set(env_t *env, const char *name, value_t *val) {
	env_entry_t *entry;
	env_t *e;

	for (e = env; e; e = e->parent) {
		for (entry = e->entries; entry; entry = entry->next) {
			if (strcmp(entry->name, name) == 0) {
				entry->val = val;
				return 0;
			}
		}
	}
	return -1;
}

value_t *env_get(env_t *env, const char *name) {
	env_entry_t *entry;
	env_t *e;

	for (e = env; e; e = e->parent) {
		for (entry = e->entries; entry; entry = entry->next) {
			if (strcmp(entry->name, name) == 0) {
				return entry->val;
			}
		}
	}
	return NULL;
}
