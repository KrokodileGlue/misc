#ifndef KTRE_H
#define KTRE_H

enum ktre_error {
	KTRE_ERROR_NO_ERROR,
	KTRE_ERROR_UNMATCHED_PAREN,
	KTRE_ERROR_STACK_OVERFLOW,
	KTRE_ERROR_STACK_UNDERFLOW
};

struct ktre {
	/* public fields */
	int num_groups, opt;
	/* string containing an error message in the case
	 * of failure during parsing or compilation */
	char *err_str;
	enum ktre_error err; /* error status code */
	_Bool failed;
	/* the location of any error that occurred, as an index
	 * in the last subject the regex was run on */
	int loc;

	/* implementation details */
	struct instr *c; /* code */
	int ip;          /* instruction pointer */
	char const *pat;
	char const *sp;  /* pointer to the character currently being parsed */
	struct node *n;
};

enum {
	KTRE_INSENSITIVE = 1 << 0,
	KTRE_UNANCHORED  = 1 << 1,
};

struct ktre *ktre_compile(const char *pat, int opt);
_Bool ktre_exec(struct ktre *re, const char *subject, int **vec);
void ktre_free(struct ktre *re);

#ifdef KTRE_IMPLEMENTATION
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef KTRE_DEBUG
#include <assert.h>
#endif

static void *
_malloc(size_t n)
{
	void *a = malloc(n);

	if (!a) {
		fprintf(stderr, "ktre: out of memory\n");
		exit(EXIT_FAILURE);
	}

	return a;
}

static void *
_calloc(size_t n)
{
	void *a = calloc(n, 1);

	if (!a) {
		fprintf(stderr, "ktre: out of memory\n");
		exit(EXIT_FAILURE);
	}

	return a;
}

static void *
_realloc(void *ptr, size_t n)
{
	void *a = realloc(ptr, n);

	if (!a) {
		fprintf(stderr, "ktre: out of memory\n");
		exit(EXIT_FAILURE);
	}

	return a;
}

struct instr {
	enum {
		INSTR_MATCH,
		INSTR_CHAR,
		INSTR_JP,
		INSTR_SPLIT,
		INSTR_ANY,
		INSTR_CLASS,
		INSTR_SPACE,
		INSTR_BACKREFERENCE,
		INSTR_BOL,
		INSTR_EOL,
		INSTR_SAVE
	} op;

	union {
		struct {
			int a, b;
		};
		int c;
		char *class;
	};
};

static void
emit_ab(struct ktre *re, int instr, int a, int b)
{
	re->c = _realloc(re->c, sizeof re->c[0] * (re->ip + 1));
	re->c[re->ip].op = instr;

	re->c[re->ip].a = a;
	re->c[re->ip].b = b;

	re->ip++;
}

static void
emit_c(struct ktre *re, int instr, int c)
{
	re->c = _realloc(re->c, sizeof re->c[0] * (re->ip + 1));
	re->c[re->ip].op = instr;
	re->c[re->ip].c = c;

	re->ip++;
}

static void
emit_class(struct ktre *re, int instr, char *class)
{
	re->c = _realloc(re->c, sizeof re->c[0] * (re->ip + 1));
	re->c[re->ip].op = instr;
	re->c[re->ip].class = class;

	re->ip++;
}

static void
emit(struct ktre *re, int instr)
{
	re->c = _realloc(re->c, sizeof re->c[0] * (re->ip + 1));
	re->c[re->ip].op = instr;
	re->ip++;
}

struct node {
	enum {
		NODE_CHAR,
		NODE_SEQUENCE,
		NODE_ASTERISK,
		NODE_PLUS,
		NODE_OR,
		NODE_GROUP,
		NODE_QUESTION,
		NODE_ANY,
		NODE_CLASS,
		NODE_SPACE,
		NODE_BACKREFERENCE,
		NODE_BOL,
		NODE_EOL,
		NODE_NONE
	} type;

	union {
		struct {
			struct node *a, *b;
		};
		int c;
		char *class;
	};
};

#define NEXT                             \
	do {                             \
		if (*re->sp) re->sp++; \
	} while (0)

#define MAX_ERROR_LEN 256 /* some arbitrary limit */

#include <stdarg.h>
static void
error(struct ktre *re, enum ktre_error err, int loc, char *fmt, ...)
{
	re->failed = true;
	re->err = err;
	re->err_str = _malloc(MAX_ERROR_LEN);
	re->loc = loc - 1;

	va_list args;
	va_start(args, fmt);
	vsnprintf(re->err_str, MAX_ERROR_LEN, fmt, args);
	va_end(args);
}

static struct node *parse(struct ktre *re);

static char *
append_to_str(char *class, char c)
{
	size_t len = class ? strlen(class) : 0;
	class = _realloc(class, len + 2);
	class[len] = c;
	class[len + 1] = 0;
	return class;
}

static struct node *
factor(struct ktre *re)
{
	struct node *left = _malloc(sizeof *left);

	/* parse a primary */
	switch (*re->sp) {
	case '\\': /* escape sequences */
		NEXT;
		switch (*re->sp) {
		case 's':
			left->type = NODE_SPACE;
			NEXT;
			break;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			left->type = NODE_BACKREFERENCE;
			left->c = *re->sp - '0';
			if (isdigit(re->sp[1])) {
				left->c *= 10;
				left->c += re->sp[1] - '0';
				NEXT;
			}
			NEXT;
			break;
		default:
			left->type = NODE_CHAR;
			left->c = *re->sp;
			NEXT;
		}
		break;

	case '[': /* character classes */
		NEXT;
		left->type = NODE_CLASS;
		char *class = NULL;
		while (*re->sp && *re->sp != ']') {
			if (re->sp[1] == '-' && re->sp[2] != ']') {
				for (int i = re->sp[0]; i < re->sp[2]; i++) {
					class = append_to_str(class, i);
				}
				re->sp += 3;
			} else {
				class = append_to_str(class, *re->sp);
				re->sp++;
			}
		}
		left->class = class;
		NEXT;
		break;
	case '(':
		NEXT;
		left->type = NODE_GROUP;
		left->a = parse(re);

		if (*re->sp != ')') {
			error(re, KTRE_ERROR_UNMATCHED_PAREN, re->sp - re->pat, "unmatched '(' at character %d", re->sp - re->pat);
			free(left->a);
			left->type = NODE_NONE;
			return left;
		}

		NEXT;
		break;
	case '.':
		NEXT;
		left->type = NODE_ANY;
		break;
	case '^':
		NEXT;
		left->type = NODE_BOL;
		break;
	case '$':
		NEXT;
		left->type = NODE_BOL;
		break;
	default:
		left->type = NODE_CHAR;
		left->c = *re->sp;
		NEXT;
	}

	while (*re->sp && (*re->sp == '*' || *re->sp == '+' || *re->sp == '?' || *re->sp == '{')) {
		NEXT;
		struct node *n = _malloc(sizeof *n);

		switch (re->sp[-1]) {
		case '*': n->type = NODE_ASTERISK; break;
		case '?': n->type = NODE_QUESTION; break;
		case '+': n->type = NODE_PLUS; break;
		case '{':
			/* NEXT; */
			/* int len = 0; */

			/* while (re->sp[len] != '}' && re->sp[len] != ',' && re->sp[len]) len++; */
			/* if (re->sp[len] == ',') { */

			/* } */
			break;
		}

		n->a = left;
		left = n;
	}

	return left;
}

static struct node *
term(struct ktre *re)
{
	struct node *left = _malloc(sizeof *left);
	left->type = NODE_NONE;

	while (*re->sp && *re->sp != '|' && *re->sp != ')') {
		struct node *right = factor(re);

		if (left->type == NODE_NONE) {
			free(left);
			left = right;
		} else {
			struct node *tmp = _malloc(sizeof *tmp);
			tmp->a = left;
			tmp->b = right;
			tmp->type = NODE_SEQUENCE;
			left = tmp;
		}
	}

	return left;
}

static struct node *
parse(struct ktre *re)
{
	struct node *n = term(re);

	if (*re->sp && *re->sp == '|') {
		NEXT;

		struct node *m = _malloc(sizeof *m);
		m->type = NODE_OR;
		m->a = n;
		m->b = parse(re);
		return m;
	}

	return n;
}

/* freenode? */
static void
free_node(struct node *n)
{
	switch (n->type) {
	case NODE_SEQUENCE: case NODE_OR:
		free_node(n->a);
		free_node(n->b);
		break;
	case NODE_ASTERISK: case NODE_PLUS:
	case NODE_GROUP: case NODE_QUESTION:
		free_node(n->a);
	default: break;
	}

	free(n);
}

#ifdef KTRE_DEBUG
#define DBG(...) fprintf(stderr, __VA_ARGS__)

static void
print_node(struct node *n)
{
#define join arm[l - 1] = 0
#define split arm[l - 1] = 1
	static int l = 0, arm[2048] = { 0 };
	DBG("\n");
	arm[l] = 0;

	for (int i = 0; i < l - 1; i++) {
		if (arm[i]) DBG("|   ");
		else DBG("    ");
	}

	if (l) {
		if (arm[l - 1]) DBG("|-- ");
		else DBG("`-- ");
	}

#define pnode(x) l++; print_node(x); l--
#define N(...) DBG(__VA_ARGS__); pnode(n->a)

	switch (n->type) {
	case NODE_SEQUENCE:
		DBG("(sequence)");
		l++;
		split;
		print_node(n->a);
		join;
		print_node(n->b);
		l--;
		break;
	case NODE_OR: DBG("(or)");
		l++;
		split;
		print_node(n->a); join;
		print_node(n->b);
		l--;
		break;
	case NODE_ASTERISK: N("(asterisk)");          break;
	case NODE_PLUS:     N("(plus)");              break;
	case NODE_GROUP:    N("(group)");             break;
	case NODE_QUESTION: N("(question)");          break;
	case NODE_ANY:      DBG("(any)");             break;
	case NODE_NONE:     DBG("(none)");            break;
	case NODE_CHAR:     DBG("(char '%c')", n->c); break;
	case NODE_BACKREFERENCE: DBG("(backreference to %d)", n->c); break;
	case NODE_CLASS:    DBG("(class '%s')", n->class); break;
	case NODE_SPACE:    DBG("(space)"); break;
	case NODE_BOL:      DBG("(bol)"); break;
	case NODE_EOL:      DBG("(eol)"); break;
	default:
		DBG("\nunimplemented printer for node of type %d\n", n->type);
		assert(false);
	}
#undef N
}
#endif

static void
compile(struct ktre *re, struct node *n)
{
#define patch_a(_re, _a, _c) _re->c[_a].a = _c;
#define patch_b(_re, _a, _c) _re->c[_a].b = _c;
#define patch_c(_re, _a, _c) _re->c[_a].c = _c;

	switch (n->type) {
	case NODE_SEQUENCE:
		compile(re, n->a);
		compile(re, n->b);
		break;

	case NODE_CHAR:
		emit_c(re, INSTR_CHAR, n->c);
		break;

	case NODE_ASTERISK: {
		int a = re->ip;
		emit_ab(re, INSTR_SPLIT, re->ip + 1, -1);
		compile(re, n->a);
		emit_ab(re, INSTR_SPLIT, a + 1, re->ip + 1);
		patch_b(re, a, re->ip);
	} break;

	case NODE_QUESTION: {
		int a;

		switch (n->a->type) {
		case NODE_ASTERISK:
			a = re->ip;
			emit_ab(re, INSTR_SPLIT, -1, re->ip + 1);
			compile(re, n->a->a);
			emit_ab(re, INSTR_SPLIT, re->ip + 1, a + 1);
			patch_a(re, a, re->ip);
			break;
		case NODE_PLUS:
			a = re->ip;
			compile(re, n->a->a);
			emit_ab(re, INSTR_SPLIT, re->ip + 1, a);
			break;
		case NODE_QUESTION:
			a = re->ip;
			emit_ab(re, INSTR_SPLIT, -1, re->ip + 1);
			compile(re, n->a->a);
			patch_a(re, a, re->ip);
			break;
		default:
			a = re->ip;
			emit_ab(re, INSTR_SPLIT, re->ip + 1, -1);
			compile(re, n->a);
			patch_b(re, a, re->ip);
		}
	} break;

	case NODE_GROUP: {
		emit_c(re, INSTR_SAVE, re->num_groups * 2);
		int old = re->num_groups;
		re->num_groups++;
		compile(re, n->a);
		re->num_groups--;
		emit_c(re, INSTR_SAVE, old * 2 + 1);
		re->num_groups++;
	} break;

	case NODE_ANY:
		emit(re, INSTR_ANY);
		break;

	case NODE_PLUS: {
		int a = re->ip;
		compile(re, n->a);
		emit_ab(re, INSTR_SPLIT, a, re->ip + 1);
	} break;

	case NODE_OR: {
		int a = re->ip;
		emit_ab(re, INSTR_SPLIT, re->ip + 1, -1);
		compile(re, n->a);
		int b = re->ip;
		emit_c(re, INSTR_JP, -1);
		patch_b(re, a, re->ip);
		compile(re, n->b);
		patch_c(re, b, re->ip);
	} break;

	case NODE_CLASS:
		emit_class(re, INSTR_CLASS, n->class);
		break;

	case NODE_SPACE:
		emit(re, INSTR_SPACE);
		break;

	case NODE_BOL:
		emit(re, INSTR_BOL);
		break;

	case NODE_EOL:
		emit(re, INSTR_EOL);
		break;

	case NODE_BACKREFERENCE:
		emit_c(re, INSTR_BACKREFERENCE,
		       n->c - 1 /* the numbers start at 1 instead of 0 */);
		break;

	default:
#ifdef KTRE_DEBUG
		DBG("\nunimplemented compiler for node of type %d\n", n->type);
		assert(false);
#endif
		break;
	}
}

struct ktre *
ktre_compile(const char *pat, int opt)
{
	struct ktre *re = _calloc(sizeof *re);
	re->pat = pat, re->opt = opt, re->sp = pat;
	re->err_str = "no error";

#ifdef KTRE_DEBUG
	DBG("\noptions set:");
	for (size_t i = 0; i < sizeof opt; i++) {
		switch (opt & 1 << i) {
		case KTRE_INSENSITIVE: DBG("\n\tINSENSITIVE"); break;
		case KTRE_UNANCHORED: DBG("\n\tUNANCHORED"); break;
		default: continue;
		}
	}
#endif

	re->n = parse(re);
	if (re->failed) {
		free_node(re->n);
		return re;
	}

	/* just emit the bytecode for .* */
	if (re->opt & KTRE_UNANCHORED) {
		emit_ab(re, INSTR_SPLIT, 1, 3);
		emit(re, INSTR_ANY);
		emit_ab(re, INSTR_SPLIT, 1, 3);
	}

	compile(re, re->n);
	if (re->failed)
		return re;

	/* just emit the bytecode for .* */
	if (re->opt & KTRE_UNANCHORED) {
		emit_ab(re, INSTR_SPLIT, re->ip + 1, re->ip + 3);
		emit(re, INSTR_ANY);
		emit_ab(re, INSTR_SPLIT, re->ip - 1, re->ip + 1);
	}

	emit(re, INSTR_MATCH);

#ifdef KTRE_DEBUG
	print_node(re->n);

	for (int i = 0; i < re->ip; i++) {
		DBG("\n%3d: ", i);

		switch (re->c[i].op) {
		case INSTR_CHAR:  DBG("CHAR   '%c'", re->c[i].c); break;
		case INSTR_SPLIT: DBG("SPLIT %3d, %3d", re->c[i].a, re->c[i].b); break;
		case INSTR_ANY:   DBG("ANY"); break;
		case INSTR_MATCH: DBG("MATCH"); break;
		case INSTR_SAVE:  DBG("SAVE  %3d", re->c[i].c); break;
		case INSTR_JP:    DBG("JP    %3d", re->c[i].c); break;
		case INSTR_CLASS: DBG("CLASS '%s'", re->c[i].class); break;
		case INSTR_SPACE: DBG("SPACE"); break;
		case INSTR_BOL:   DBG("BOL"); break;
		case INSTR_EOL:   DBG("EOL"); break;
		case INSTR_BACKREFERENCE: DBG("BACKREF %d", re->c[i].c); break;
		default: assert(false);
		}
	}
#endif
	free_node(re->n);

	return re;
}

static inline char
lc(char c)
{
	if (c >= 'A' && c <= 'Z') {
		c = (c - 'A') + 'a';
	}

	return c;
}

#define MAX_THREAD (1 << 11) /* 2048 should be enough */
static bool
run(struct ktre *re, const char *subject, int *vec)
{
	struct thread {
		int ip, sp;
		int old, old_idx;
	} t[MAX_THREAD];
	int tp = 0; /* toilet paper */

#define new_thread(_ip, _sp) \
	t[++tp] = (struct thread){ _ip, _sp, -1, -1 }

	/* push the initial thread */
	new_thread(0, 0);

	while (tp) {
		int ip = t[tp].ip, sp = t[tp].sp;
//		printf("\nip: %d, sp: %d, tp: %d", ip, sp, tp);

		if (tp == MAX_THREAD - 1) {
			error(re, KTRE_ERROR_STACK_OVERFLOW, 0, "regex exceeded the maximum number of executable threads");
			return false;
		}

		if (tp <= 0) {
			error(re, KTRE_ERROR_STACK_UNDERFLOW, 0, "regex killed more threads than it started");
			return false;
		}

		switch (re->c[ip].op) {
		case INSTR_BACKREFERENCE:
			tp--;
			if (!strncmp(subject + sp, &subject[vec[re->c[ip].c * 2]], vec[re->c[ip].c * 2 + 1]))
				new_thread(ip + 1, sp + vec[re->c[ip].c * 2 + 1]);
			break;

		case INSTR_CLASS:
			tp--;
			if (strchr(re->c[ip].class, subject[sp])) new_thread(ip + 1, sp + 1);
			break;

		case INSTR_SPACE:
			tp--;
			if (isspace(subject[sp])) new_thread(ip + 1, sp + 1);
			break;

		case INSTR_BOL:
			tp--;
			if (sp == 0) new_thread(ip + 1, sp);
			break;

		case INSTR_EOL:
			tp--;
			if (subject[sp + 1] == 0 || subject[sp + 1] == '\n') new_thread(ip + 1, sp);
			break;

		case INSTR_CHAR:
			tp--;
			if (re->opt & KTRE_INSENSITIVE) {
				if (lc(subject[sp]) == lc(re->c[ip].c)) {
					new_thread(ip + 1, sp + 1);
				}
			} else {
				if (subject[sp] == re->c[ip].c) {
					new_thread(ip + 1, sp + 1);
				}
			}
			break;

		case INSTR_ANY:
			tp--;
			if (subject[sp] != 0) new_thread(ip + 1, sp + 1);
			break;

		case INSTR_SPLIT:
			t[tp].ip = re->c[ip].b;
			new_thread(re->c[ip].a, sp);
			break;

		case INSTR_MATCH:
			if (subject[sp] == 0)
				return true;
			tp--;
			break;

		case INSTR_SAVE:
			if (t[tp].old == -1) {
				t[tp].old = vec[re->c[ip].c];
				t[tp].old_idx = re->c[ip].c;
				if (re->c[ip].c % 2 == 0)
					vec[re->c[ip].c] = sp;
				else
					vec[re->c[ip].c] = sp - vec[re->c[ip].c - 1];
				new_thread(ip + 1, sp);
			} else {
				vec[t[tp].old_idx] = t[tp].old;
				tp--;
			}
			break;

		case INSTR_JP:
			t[tp].ip = re->c[ip].c;
			break;

		default:
#ifdef KTRE_DEBUG
			DBG("\nunimplemented instruction %d\n", re->c[ip].op);
			assert(false);
#endif
			return false;
		}
	}

	return false;
}

void
ktre_free(struct ktre *re)
{
	for (int i = 0; i < re->ip; i++)
		if (re->c[i].op == INSTR_CLASS)
			free(re->c[i].class);

	free(re->c);
	if (re->err) free(re->err_str);
	free(re);
}

_Bool
ktre_exec(struct ktre *re, const char *subject, int **vec)
{
	*vec = _calloc(sizeof (*vec[0]) * re->num_groups * 2);

	if (run(re, subject, *vec)) {
		return true;
	} else {
		free(*vec);
		return false;
	}
}
#endif /* ifdef KTRE_IMPLEMENTATION */
#endif /* ifndef KTRE_H */
