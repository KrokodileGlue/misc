#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* removes dead code, comments, redundant pointer movement and
 * redundant arithmetic commands in brainfuck programs. */
void sanitize(char* str)
{
#define BFSAN_IS_BF_COMMAND(c) ( \
        c == '<' || c == '>'     \
        || c == '+' || c == '-'  \
        || c == '[' || c == ']'  \
        || c == ',' || c == '.')

#define BFSAN_ADD(a, c)                                              \
        if (a >= 0) {                                                \
                for (int counter = 0; counter < abs(a); counter++) { \
                        *c++ = '+';                                  \
                }                                                    \
        } else {                                                     \
    	        for (int counter = 0; counter < abs(a); counter++) { \
                        *c++ = '-';                                  \
                }                                                    \
        }

#define BFSAN_MOVE_PTR(a, c)                                       \
        if (a >= 0)                                                \
                for (int counter = 0; counter < abs(a); counter++) \
                        *c++ = '>';                                \
        else                                                       \
                for (int counter = 0; counter < abs(a); counter++) \
                        *c++ = '<';

	if (!str) return;

	size_t starting_len = strlen(str);
	char* i = str, *out = str;

	while (*i) {
		if (*i == '+' || *i == '-' || *i == '<' || *i == '>') {
			if (*i == '+' || *i == '-') {
				int sum = 0;
				while (*i == '+' || *i == '-') {
					if (*i == '+') sum++;
					else sum--;

					i++;
				}
				BFSAN_ADD(sum, out)
			} else {
				int sum = 0;
				while (*i == '>' || *i == '<') {
					if (*i == '>') sum++;
					else sum--;

					i++;
				}
				BFSAN_MOVE_PTR(sum, out)
			}
		} else if (!strncmp(i, "][", 2)) {
			i += 2;
			int depth = 1;
			do {
				i++;
				if (*i == '[') depth++;
				else if (*i == ']') depth--;
			} while (depth && *i);
		}
		else if (BFSAN_IS_BF_COMMAND(*i)) *out++ = *i++;
		else i++;
	}
	*out = 0;

	if (strlen(str) < starting_len) sanitize(str);
#undef BFSAN_IS_BF_COMMAND
#undef BFSAN_ADD
#undef BFSAN_MOVE_PTR
}

int main()
{
	char test[] = "+++++ comments! +++++ redundant addition/subtraction: -----+++++ [>++++++++++<-] dead code: [-] [-] redundant pointer movements: >>>>><<<<++++.";
	printf("before: \"%s\"\n", test);
	sanitize(test);
	printf("after: \"%s\"\n", test);

	return EXIT_SUCCESS;
}
