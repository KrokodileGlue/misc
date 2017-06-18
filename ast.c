#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

struct Op {
	char body;
	char body2;
	int prec;
	enum {
		LEFT,
		RIGHT
	} ass;
	enum OpType {
		/* prefix operator types */
		PREFIX,
		GROUP, /* only used for grouping with () */

		/* infix operator types */
		POSTFIX,
		BINARY,
		MEMBER,
		TERNARY
	} type;
} operators[] = {
	{ '(', ')', 7, LEFT,  MEMBER  },
	{ '[', ']', 7, LEFT,  MEMBER  },
	{ '+', 'n', 6, RIGHT, PREFIX  },
	{ '-', 'n', 6, RIGHT, PREFIX  },
	{ '(', ')', 6, LEFT,  GROUP   },
	{ '*', 'n', 5, LEFT,  BINARY  },
	{ '/', 'n', 5, LEFT,  BINARY  },
	{ '%', 'n', 5, LEFT,  BINARY  },
	{ '+', 'n', 4, LEFT,  BINARY  },
	{ '-', 'n', 4, LEFT,  BINARY  },
	{ '?', ':', 3, LEFT,  TERNARY },
	{ '=', 'n', 2, LEFT,  BINARY  },
	{ ',', 'n', 1, RIGHT, BINARY  }
};

/* a(a,b)=(a*b),pa(2,4) */
/* i(x)=((x)w(p(x%(2*5)),x=x/(2*5))),s(x)=(a=0,!(x[a]~0)w(p(x[a]),a=a+1),p(2*5)),a=0,(!(a=a+1~(5*5*4+1)))w(a%3~0&a%5~0?s("fizzbuzz"):a%3~0?s("fizz"):a%5~0?s("buzz"):i(a)) */

struct Expr {
	enum ExprType {
		EXPR_NAME,
		EXPR_NUM,
		EXPR_OP
	} type;
	struct Op *op;
	union {
		char val;
		struct {
			struct Expr *a, *b, *c;
		};
	};
};

char *c;

struct Op *get_infix_op()
{
	for (size_t i = 0; i < sizeof operators / sizeof operators[0]; i++) {
		if (operators[i].body == *c
		    && operators[i].type != PREFIX) {
			return operators + i;
		}
	}
	return NULL;
}

struct Op *get_prefix_op()
{
	for (size_t i = 0; i < sizeof operators / sizeof operators[0]; i++) {
		if (operators[i].body == *c
		    && (operators[i].type == PREFIX
		    ||  operators[i].type == GROUP)) {
			return operators + i;
		}
	}
	return NULL;
}

int get_prec(int prec)
{
	if (get_infix_op()) return get_infix_op()->prec;
	return prec;
}

struct Expr *parse(int prec)
{
	struct Op *op = get_prefix_op();
	struct Expr *left = malloc(sizeof *left);

	if (op) {
		left->type = EXPR_OP;
		left->op = op;

		c++;
		if (op->type == GROUP) {
			left->a = parse(0);
			if (*c != op->body2) {
				printf("(prefix) expected '%c', got '%c'\n",op->body2, *c);
				exit(EXIT_FAILURE);
			}
			c++;
		} else {
			left->a = parse(op->prec);
		}
	} else {
		if (isalpha(*c)) {
			left->type = EXPR_NAME;
			left->val = *c;
			c++;
		} else if (isdigit(*c)) {
			left->type = EXPR_NUM;
			left->val = *c - '0';
			c++;
		}
	}

	op = NULL;
	struct Expr *e = left;

	while (prec < get_prec(prec)) {
		op = get_infix_op();
		if (!op) return left;

		e = malloc(sizeof *e);
		e->type = EXPR_OP;
		e->op = op;
		e->a = left;

		if (op->type == TERNARY) {
			c++;
			e->b = parse(1);
			if (*c != op->body2) {
				printf("expected '%c', got '%c'\n", op->body2, *c);
				exit(EXIT_FAILURE);
			}
			c++;
			e->c = parse(1);
		} else if (op->type == MEMBER) {
			c++;
			e->b = parse(0);
			if (*c != op->body2) {
				printf("(prefix) expected '%c', got '%c'\n",op->body2, *c);
				exit(EXIT_FAILURE);
			}
			c++;
		} else if (op->type == BINARY) {
			c++;
			e->b = parse(op->ass == LEFT
					  ? op->prec
					  : op->prec - 1);
		} else if (op->type == POSTFIX) {
			c++;
		}

		left = e;
	}

	return e;
}

void paren(struct Expr *e)
{
	switch (e->type) {
	case EXPR_NAME: printf("%c", e->val); break;
	case EXPR_NUM:  printf("%c", e->val + '0'); break;
	case EXPR_OP: {
		switch (e->op->type) {
		case GROUP: {
			paren(e->a);
		} break;
		case PREFIX: {
			printf("(%c", e->op->body);
			paren(e->a);
			printf(")");
		} break;
		case BINARY: {
			printf("(");
			paren(e->a);
			printf("%c", e->op->body);
			paren(e->b);
			printf(")");
		} break;
		case TERNARY: {
			printf("(");
			paren(e->a);
			printf("%c", e->op->body);
			paren(e->b);
			printf("%c", e->op->body2);
			paren(e->c);
			printf(")");
		} break;
		case POSTFIX: {
			printf("(");
			paren(e->a);
			printf("%c)", e->op->body);
		} break;
		case MEMBER: {
			printf("(");
			paren(e->a);
			printf("%c", e->op->body);
			paren(e->b);
			printf("%c)", e->op->body2);
		} break;
		}
	} break;
	}
}

int l = 0, arm[2048];

void indent()
{
	printf("\n\t");
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

void split()
{
	arm[l - 1] = 1;
}

void join()
{
	arm[l - 1] = 0;
}

void tree(struct Expr *e)
{
	indent();

	switch (e->type) {
	case EXPR_NAME: printf("%c", e->val); break;
	case EXPR_NUM:  printf("%c", e->val + '0'); break;
	case EXPR_OP: {
		switch (e->op->type) {
		case GROUP: {
			printf("(group)");
			l++; tree(e->a);
			l--;
		} break;
		case PREFIX: {
			printf("(prefix %c)", e->op->body);
			l++;
			tree(e->a);
			l--;
		} break;
		case BINARY: {
			printf("(binary %c)", e->op->body);
			l++;
			split(); tree(e->a);
			join();  tree(e->b);
			l--;
		} break;
		case TERNARY: {
			printf("(ternary %c)", e->op->body);
			l++;
			split();
			tree(e->a);
			tree(e->b);
			join();
			tree(e->c);
			l--;
		} break;
		case POSTFIX: {
			printf("(");
			paren(e->a);
			printf(" %c)", e->op->body);
		} break;
		case MEMBER: {
			printf("(member of "); paren(e->a); printf(")");
			l++;
			tree(e->b);
			l--;
		} break;
		}
	} break;
	}
}

int main(int argc, char **argv)
{
	printf("expression:\n\t%s\n", argv[1]);
	c = argv[1];
	struct Expr *e = parse(0);

	printf("syntax tree:");
	tree(e);

	printf("\nparenthesized:\n\t");
	paren(e);

	printf("\n");

	return 0;
}
