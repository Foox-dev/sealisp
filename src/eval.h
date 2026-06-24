#pragma once

#include "env.h"
#include "value.h"

value_t *eval(value_t *expr, env_t *env);
value_t *apply(value_t *fn, value_t *args, env_t *env);
