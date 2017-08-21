#include <stdlib.h>
#include <stdio.h>

#define KTRE_IMPLEMENTATION
#include "ktre.h"

#define MAX_GROUPS 256

int main(int argc, char *argv[])
{
	char *subject = argv[1], *regex = argv[2];
	printf("matching string '%s' against regex '%s'", subject, regex);
	int vec[MAX_GROUPS];
	struct ktre *re = ktre_compile(regex, 0);

	if (ktre_exec(re, 0, subject, vec, MAX_GROUPS)) {
		printf("matched!\n");

		for (int i = 0; i < re->num_groups; i++) {
			printf("group %d: %.*s\n", i, vec[i * 2 + 1], &subject[vec[i * 2]]);
		}
	} else {
		printf("did not match!\n");
	}

	return EXIT_SUCCESS;
}
