#include <stdio.h>
#include <string.h>

#include "builtins.h"
#include "eval.h"
#include "printer.h"
#include "reader.h"

#define INPUT_BUF_MAX 4096
#define LINE_BUF_MAX 1024

static void repl(env_t *env);

static void repl(env_t *env) {
	char input[INPUT_BUF_MAX];
	char line[LINE_BUF_MAX];
	reader_t r;
	value_t *form;
	value_t *result;

	input[0] = '\0';
	for (;;) {
		printf(strlen(input) == 0 ? "sealisp> " : "...... > ");
		fflush(stdout);

		if (!fgets(line, sizeof(line), stdin)) {
			printf("\n");
			return;
		}

		strncat(input, line, sizeof(input) - strlen(input) - 1);

		if (!reader_is_complete(input)) {
			continue;
		}

		r = reader_new(input);
		while ((form = reader_read(&r)) != NULL) {
			result = eval(form, env);
			value_print(stdout, result);
			printf("\n");
		}

		input[0] = '\0';
	}
}

int main() {
	env_t *env;

	env = env_new(NULL);
	builtins_install(env);
	repl(env);
	return 0;
}
