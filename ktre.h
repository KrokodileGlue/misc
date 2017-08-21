#ifndef KTRE_H
#define KTRE_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#define KTRE_DEBUG 1

struct ktre {
	struct instr *c; /* code */
	size_t ip;       /* instruction pointer */
	int num_groups;  /* group pointer */
};

enum {
	KTRE_INSENSITIVE = 1
};

struct ktre *ktre_compile(const char *pat, int opt);
bool ktre_exec(const struct ktre *re, int opt, const char *subject, int *vec, int vec_len);

#ifdef KTRE_IMPLEMENTATION
struct instr {
	enum {
		INSTR_MATCH,
		INSTR_CHAR,
		INSTR_JP,
		INSTR_SPLIT,
		INSTR_ANY,
		INSTR_SAVE
	} op;

	union {
		struct {
			uint16_t a, b;
		};
		char c;
	};
};

static void
emit_ab(struct ktre *re, int instr, uint16_t a, uint16_t b)
{
	re->c = realloc(re->c, sizeof (re->c[0]) * re->ip + sizeof re->c[0]);
	re->c[re->ip].op = instr;

	re->c[re->ip].a = a;
	re->c[re->ip].b = b;

	re->ip++;
}

static void
emit_c(struct ktre *re, int instr, uint8_t c)
{
	re->c = realloc(re->c, sizeof (re->c[0]) * re->ip + sizeof re->c[0]);
	re->c[re->ip].op = instr;
	re->c[re->ip].c = c;

	re->ip++;
}

static void
emit(struct ktre *re, int instr)
{
	re->c = realloc(re->c, sizeof (re->c[0]) * re->ip + sizeof re->c[0]);
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
		NODE_NONE
	} type;

	struct node *a, *b;
	int c;
};

static struct node *parse(const char **pat);

#define EXPECT(x)	  \
	do { \
		if (**pat != x) { \
			fprintf(stderr, "unexpected token %c; expected %c\n", **pat, x); \
		} \
	} while (false)

static struct node *
base(const char **pat)
{
	struct node *n = malloc(sizeof *n);

	switch (**pat) {
	case '\\':
		(*pat)++;
		n->type = NODE_CHAR;
		n->c = **pat;
		break;
	case '(':
		(*pat)++;
		struct node *r = malloc(sizeof *r);
		r->type = NODE_GROUP;
		r->a = parse(pat);

		EXPECT(')');
		(*pat)++;
		return r;
	case '.':
		(*pat)++;
		n->type = NODE_ANY;
		break;
	default:
		n->type = NODE_CHAR;
		n->c = **pat;
		(*pat)++;
	}

	return n;
}

static struct node *
factor(const char **pat)
{
	struct node *left = base(pat);

	while (**pat && (**pat == '*' || **pat == '+' || **pat == '?')) {
		(*pat)++;
		struct node *n = malloc(sizeof *n);

		if ((*pat)[-1] == '*')
			n->type = NODE_ASTERISK;
		else if ((*pat)[-1] == '?')
			n->type = NODE_QUESTION;
		else
			n->type = NODE_PLUS;

		n->a = left;
		left = n;
	}

	return left;
}

static struct node *
term(const char **pat)
{
	struct node *left = malloc(sizeof *left);
	left->type = NODE_NONE;

	while (**pat && **pat != '|' && **pat != ')') {
		struct node *right = factor(pat);

		if (left->type == NODE_NONE) {
			left = right;
		} else {
			struct node *tmp = malloc(sizeof *tmp);
			tmp->a = left;
			tmp->b = right;
			tmp->type = NODE_SEQUENCE;
			left = tmp;
		}
	}

	return left;
}

static struct node *
parse(const char **pat)
{
	struct node *n = term(pat);

	if (**pat && **pat == '|') {
		(*pat)++;

		struct node *m = malloc(sizeof *m);
		m->type = NODE_OR;
		m->a = n;
		m->b = parse(pat);
		return m;
	} else {
		return n;
	}

	return n;
}

#ifdef KTRE_DEBUG
static int l = 0, arm[2048];

static void
indent()
{
	printf("\n");
	arm[l] = 0;

	for (int i = 0; i < l - 1; i++) {
		if (arm[i]) printf("|   ");
		else printf("    ");
	}

	if (l) {
		if (arm[l - 1]) printf("|-- ");
		else printf("`-- ");
	}
}

static void
split()
{
	arm[l - 1] = 1;
}

static void
join()
{
	arm[l - 1] = 0;
}

static void
print_node(struct node *n)
{
	indent();

	switch (n->type) {
	case NODE_CHAR:
		printf("(char '%c')", n->c);
		break;
	case NODE_SEQUENCE:
		printf("(sequence)");
		l++;
		split();
		print_node(n->a);
		join();
		print_node(n->b);
		l--;
		break;
	case NODE_NONE:
		printf("(none)");
		break;
	case NODE_OR:
		printf("(or)");
		l++;
		split();
		print_node(n->a);
		join();
		print_node(n->b);
		l--;
		break;
	case NODE_ASTERISK:
		printf("(asterisk)");
		l++;
		print_node(n->a);
		l--;
		break;
	case NODE_PLUS:
		printf("(plus)");
		l++;
		print_node(n->a);
		l--;
		break;
	case NODE_GROUP:
		printf("(group)");
		l++;
		print_node(n->a);
		l--;
		break;
	case NODE_QUESTION:
		printf("(question)");
		l++;
		print_node(n->a);
		l--;
		break;
	case NODE_ANY:
		printf("(any)");
		break;
	default:
		printf("unimplemented printer for node of type %d\n", n->type);
		assert(false);
	}
}
#endif

#define patch_b(_re, _a, _c)	  \
	_re->c[_a].b = _c;

static void
compile(struct ktre *re, struct node *n)
{
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
		int a = re->ip;
		emit_ab(re, INSTR_SPLIT, re->ip + 1, -1);
		compile(re, n->a);
		patch_b(re, a, re->ip);
	} break;
	case NODE_GROUP: {
		emit_c(re, INSTR_SAVE, re->num_groups * 2);
		compile(re, n->a);
		emit_c(re, INSTR_SAVE, re->num_groups * 2 + 1);
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
	default:
		assert(false);
	}
}

struct ktre *
ktre_compile(const char *pat, int opt)
{
	struct ktre *re = malloc(sizeof *re);
	re->c = NULL, re->ip = 0;

	/* parse */
	struct node *n = parse(&pat);
#ifdef KTRE_DEBUG
	print_node(n); putchar('\n');
#endif
	/* compile */
	compile(re, n);
	emit(re, INSTR_MATCH);

#ifdef KTRE_DEBUG
	for (int i = 0; i < re->ip; i++) {
		printf("%3d: ", i);

		switch (re->c[i].op) {
		case INSTR_CHAR:  printf("CHAR   '%c'\n", re->c[i].c); break;
		case INSTR_SPLIT: printf("SPLIT %3d, %3d\n", re->c[i].a, re->c[i].b); break;
		case INSTR_ANY:   printf("ANY\n"); break;
		case INSTR_MATCH: printf("MATCH\n"); break;
		case INSTR_SAVE:  printf("SAVE  %3d\n", re->c[i].c); break;
		default: assert(false);
		}
	}
#endif

	return re;
}

static bool
run(const struct ktre *re, const char *subject, int ip, int sp, int *vec, int vec_len)
{
	switch (re->c[ip].op) {
	case INSTR_CHAR:
		if (subject[sp] != re->c[ip].c)
			return false;

		return run(re, subject, ip + 1, sp + 1, vec, vec_len);
		break;
	case INSTR_ANY:
		if (subject[sp] == 0)
			return false;
		else
			return run(re, subject, ip + 1, sp + 1, vec, vec_len);
	case INSTR_SPLIT:
		if (run(re, subject, re->c[ip].a, sp, vec, vec_len))
			return true;

		return run(re, subject, re->c[ip].b, sp, vec, vec_len);
	case INSTR_MATCH:
		return true;
	case INSTR_SAVE: {
		int old = vec[re->c[ip].c];

		if (re->c[ip].c % 2 == 0)
			vec[re->c[ip].c] = sp;
		else
			vec[re->c[ip].c] = sp - vec[re->c[ip].c - 1];

		if (run(re, subject, ip + 1, sp, vec, vec_len)) {
			return true;
		}

		vec[re->c[ip].c] = old;
		return false;
	} break;
	default:
		printf("unimplemented instruction %d\n", re->c[ip].c);
		return false;
	}

	assert(false);
	return false;
}

bool
ktre_exec(const struct ktre *re, int opt, const char *subject, int *vec, int vec_len)
{
	return run(re, subject, 0, 0, vec, vec_len);
}
#endif /* ifdef KTRE_IMPLEMENTATION */
#endif /* ifndef KTRE_H */
