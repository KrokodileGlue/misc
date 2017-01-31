#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* cleans up brainfuck by removing dead code, comments,
 * and redundant pointer movement / cell arithmetic commands. */

void sanitize(char* str)
{
	if (!str)
		return;

#define SANITIZER_IS_BF_COMMAND(c) (   \
        c == '<' || c == '>'     \
        || c == '+' || c == '-'  \
        || c == '[' || c == ']'  \
        || c == ',' || c == '.')

#define SANITIZER_ADD(a, c) \
        if (a >= 0) { \
                for (int counter = 0; counter < abs(a); counter++) { \
                        *c++ = '+'; \
                } \
        } else { \
    	        for (int counter = 0; counter < abs(a); counter++) { \
                        *c++ = '-'; \
                } \
        }

#define SANITIZER_MOVE_PTR(a, c) \
    if (a >= 0) { \
        for (int counter = 0; counter < abs(a); counter++) { \
            *c++ = '>'; \
        } \
    } else { \
    	for (int counter = 0; counter < abs(a); counter++) { \
            *c++ = '<'; \
        } \
    }

#define SANITIZER_IS_CONTRACTABLE(c) (     \
        c == '<' || c == '>'     \
        || c == '+' || c == '-')

	char* buf = malloc(strlen(str) + 1);

	size_t starting_len;
	char* i, *out;

	starting_len = strlen(str);
	i = str, out = buf;

	while (*i != '\0') {
		if (SANITIZER_IS_CONTRACTABLE(*i)) {
			if (*i == '+' || *i == '-') {
				int sum = 0;
				while (*i == '+' || *i == '-') {
					if (*i == '+') sum++;
					else sum--;
					i++;
				}
				SANITIZER_ADD(sum, out)
			} else {
				int sum = 0;
				while (*i == '>' || *i == '<') {
					if (*i == '>') sum++;
					else sum--;
					i++;
				}
				SANITIZER_MOVE_PTR(sum, out)
			}
		} else if (!strncmp(i, "][", 2)) {
			i += 2;
			int depth = 1;
			while (*i != '\0' && depth) {
				if (*i == '[') depth++;
				else if (*i == ']') depth--;
				i++;
			}
			i--;
		} else if (SANITIZER_IS_BF_COMMAND(*i)) {
			*out++ = *i++;
		} else {
			i++;
		}
	}
	*out = '\0';
	strcpy(str, buf);
	free(buf);

	if (strlen(str) < starting_len) {
		sanitize(str);
	}
}

int main()
{
	char test[] = "++++++++++++++----[>++++++++++<-][-][-][-]><><><>><><++++++++----.";
	printf("before: \"%s\"\n", test);
	sanitize(test);
	printf("after: \"%s\"\n", test);

	return EXIT_SUCCESS;
}