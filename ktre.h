#ifndef KTRE_H
#define KTRE_H

struct ktre {
	struct instr *c; /* code */
	int ip;          /* instruction pointer */
	int num_groups, opt;
};

enum {
	KTRE_INSENSITIVE = 1,
	KTRE_UNANCHORED
};

struct ktre *ktre_compile(const char *pat, int opt);
_Bool ktre_exec(const struct ktre *re, const char *subject, int **vec);
void ktre_free(struct ktre *re);

#ifdef KTRE_IMPLEMENTATION
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

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
		INSTR_SAVE
	} op;

	union {
		struct {
			int a, b;
		};
		int c;
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
		NODE_NONE
	} type;

	union {
		struct {
			struct node *a, *b;
		};
		int c;
	};
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
	struct node *n = _malloc(sizeof *n);

	switch (**pat) {
	case '\\':
		(*pat)++;
		n->type = NODE_CHAR;
		n->c = **pat;
		break;
	case '(':
		(*pat)++;
		n->type = NODE_GROUP;
		n->a = parse(pat);
		(*pat)++;
		break;
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
		struct node *n = _malloc(sizeof *n);

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
	struct node *left = _malloc(sizeof *left);
	left->type = NODE_NONE;

	while (**pat && **pat != '|' && **pat != ')') {
		struct node *right = factor(pat);

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
parse(const char **pat)
{
	struct node *n = term(pat);

	if (**pat && **pat == '|') {
		(*pat)++;

		struct node *m = _malloc(sizeof *m);
		m->type = NODE_OR;
		m->a = n;
		m->b = parse(pat);
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
	static int l = 0, arm[2048];
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
	default:
		DBG("unimplemented printer for node of type %d\n", n->type);
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

	default:
		assert(false);
	}
}

struct ktre *
ktre_compile(const char *pat, int opt)
{
	struct ktre *re = _malloc(sizeof *re);
	re->c = NULL, re->ip = 0, re->opt = opt, re->num_groups = 0;

	/* parse */
	struct node *n = parse(&pat);

	/* compile */
	compile(re, n);
	emit(re, INSTR_MATCH);

#ifdef KTRE_DEBUG
	print_node(n); DBG("\n");

	for (int i = 0; i < re->ip; i++) {
		DBG("\n%3d: ", i);

		switch (re->c[i].op) {
		case INSTR_CHAR:  DBG("CHAR   '%c'", re->c[i].c); break;
		case INSTR_SPLIT: DBG("SPLIT %3d, %3d", re->c[i].a, re->c[i].b); break;
		case INSTR_ANY:   DBG("ANY"); break;
		case INSTR_MATCH: DBG("MATCH"); break;
		case INSTR_SAVE:  DBG("SAVE  %3d", re->c[i].c); break;
		default: assert(false);
		}
	}
#endif
	free_node(n);

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
		return subject[sp] == 0;
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
#ifdef KTRE_DEBUG
		DBG("unimplemented instruction %d\n", re->c[ip].c);
#endif
		return false;
	}

	assert(false);
	return false;
}

void
ktre_free(struct ktre *re)
{
	free(re->c);
	free(re);
}

_Bool
ktre_exec(const struct ktre *re, const char *subject, int **vec)
{
	*vec = _malloc(sizeof (*vec[0]) * re->num_groups * 2);
	memset(*vec, 0, sizeof (*vec[0]) * re->num_groups * 2);

	if (run(re, subject, 0, 0, *vec, re->num_groups * 2)) {
		return true;
	} else {
		free(*vec);
		return false;
	}
}
#endif /* ifdef KTRE_IMPLEMENTATION */
#endif /* ifndef KTRE_H */
