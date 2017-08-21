#include <stdlib.h>
#include <stdio.h>

#define KTRE_IMPLEMENTATION
#include "ktre.h"

#define MAX_GROUPS 256

int main(int argc, char *argv[])
{
	char *regex = "(fo*)(.*)(ba?r)", *subject = "foo______br";
	printf("matching string '%s' against regex '%s'", subject, regex);
	int vec[MAX_GROUPS];
	struct ktre *re = ktre_compile(regex, 0);

	if (ktre_exec(re, 0, subject, vec, MAX_GROUPS)) {
		printf("matched!\n");

		for (int i = 0; i < re->num_groups; i += 2) {
			printf("group %d: %.*s\n", i / 2, vec[i + 1], &subject[vec[i]]);
		}
	} else {
		printf("did not match!\n");
	}

	return EXIT_SUCCESS;
}
