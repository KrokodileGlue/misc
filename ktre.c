#include <stdlib.h>
#include <stdio.h>

#define KTRE_DEBUG
#define KTRE_IMPLEMENTATION
#include "ktre.h"

int main(int argc, char *argv[])
{
	if (argc < 3) {
		fprintf(stderr, "Usage: ktre subject pattern\n");
		exit(EXIT_FAILURE);
	}

	char *subject = argv[1], *regex = argv[2];
	fprintf(stderr, "matching string '%s' against regex '%s'", subject, regex);

	int *vec = NULL;
	struct ktre *re = ktre_compile(regex, 0);

	if (ktre_exec(re, subject, &vec)) {
		printf("\nmatched!");

		for (int i = 0; i < re->num_groups; i++)
			printf("\ngroup %d: %.*s", i, vec[i * 2 + 1], &subject[vec[i * 2]]);

		putchar('\n');
		free(vec);
	} else {
		printf("\ndid not match!");
	}

	ktre_free(re);

	return EXIT_SUCCESS;
}
