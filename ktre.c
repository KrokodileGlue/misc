#include <stdlib.h>
#include <stdio.h>

#define KTRE_DEBUG
#define KTRE_IMPLEMENTATION
#include "ktre.h"

int main(int argc, char *argv[])
{
	if (argc != 3) {
		fprintf(stderr, "Usage: ktre [subject] [pattern]\n");
		exit(EXIT_FAILURE);
	}

	char *subject = argv[1], *regex = argv[2];
	fprintf(stderr, "matching string '%s' against regex '%s'", subject, regex);

	int *vec = NULL;
	struct ktre *re = ktre_compile(regex, KTRE_INSENSITIVE | KTRE_UNANCHORED);

	if (re->err) { /* failed to compile */
		printf("\nregex failed to compile with error code %d: %s\n", re->err, re->err_str);
		printf("\t%s\n\t", regex);
		for (int i = 0; i < re->loc; i++) putchar(' ');
		printf("^\n");
	} else if (ktre_exec(re, subject, &vec)) { /* ran and succeeded */
		printf("\nmatched!");

		for (int i = 0; i < re->num_groups; i++)
			printf("\ngroup %d: '%.*s'", i, vec[i * 2 + 1], &subject[vec[i * 2]]);

		putchar('\n');
		free(vec);
	} else if (re->err) { /* ran, but failed with an error */
		printf("\nregex failed at runtime with error code %d: %s\n", re->err, re->err_str);

		printf("\t%s\n\t", regex);
		for (int i = 0; i < re->loc; i++) putchar(' ');
		printf("^\n");
	} else { /* ran, but failed */
		printf("\ndid not match!\n");
	}

	ktre_free(re);

	return EXIT_SUCCESS;
}
