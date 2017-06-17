#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

struct Op {
	char body;
	int prec;
	enum {
		LEFT,
		RIGHT
	} ass;
	enum OpType {
		PREFIX,
		BINARY,
		INFIX,
		POSTFIX
	} type;
} operators[] = {
	{ '!', 7, RIGHT, POSTFIX },
	{ '(', 7, LEFT,  INFIX   },
	{ '+', 6, RIGHT, PREFIX  },
	{ '-', 6, RIGHT, PREFIX  },
	{ '(', 6, LEFT,  PREFIX  },
	{ '*', 5, LEFT,  BINARY  },
	{ '/', 5, LEFT,  BINARY  },
	{ '+', 4, LEFT,  BINARY  },
	{ '-', 4, LEFT,  BINARY  },
	{ '?', 3, LEFT,  INFIX   },
	{ '=', 2, LEFT,  BINARY  },
	{ ',', 1, RIGHT, BINARY  }
};

struct Expr {
	enum ExprType {
		EXPR_VALUE,
		EXPR_PREFIX,
		EXPR_BINARY,
		EXPR_POSTFIX,
		EXPR_TERNARY,
		EXPR_CALL
	} type;
	char op;
	union {
		struct Expr *unary_operand;
		struct {
			struct Expr *left, *right;
		};
		char val;
		struct {
			struct Expr *condition, *then_branch, *else_branch;
		};
		struct {
			struct Expr *call, **args;
			size_t len;
		};
	};
};

char *c;

struct Op *get_infix_op()
{
	for (size_t i = 0; i < sizeof operators / sizeof operators[0]; i++) {
		if (operators[i].body == *c && (operators[i].type == INFIX || operators[i].type == BINARY || operators[i].type == POSTFIX)) {
			return operators + i;
		}
	}
	return NULL;
}

struct Op *get_prefix_op()
{
	for (size_t i = 0; i < sizeof operators / sizeof operators[0]; i++) {
		if (operators[i].body == *c && operators[i].type == PREFIX) {
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

struct Expr *parse_value()
{
	struct Expr *e = malloc(sizeof *e);
	if (isalnum(*c)) {
		e->type = EXPR_VALUE;
		e->val = *c;
		c++;
	} else {
		printf("could not parse value of '%c'.\n", *c);
		exit(EXIT_FAILURE);
	}

	return e;
}

struct Expr *parse(int prec)
{
	struct Op *prefix_op = get_prefix_op();
	struct Expr *left = NULL;

	if (prefix_op) {
		left = malloc(sizeof *left);
		left->type = EXPR_PREFIX;
		left->op = *c;
		c++;
		if (left->op == '(') {
			left->unary_operand = parse(0);
			if (*c != ')') {
				printf("(prefix) expected ')', got '%c'\n", *c);
				exit(EXIT_FAILURE);
			}
			c++;
		} else {
			left->unary_operand = parse(prefix_op->prec);
		}
	} else {
		left = parse_value();
	}

	struct Op *infix_op = NULL;
	struct Expr *e = left;

	while (prec < get_prec(prec)) {
		infix_op = get_infix_op();
		if (!infix_op) return left;

		e = malloc(sizeof *e);

		if (*c == '?') {
			c++;
			e->type = EXPR_TERNARY;
			e->condition = left;
			e->then_branch = parse(1);
			if (*c != ':') {
				printf("expected ':', got '%c'\n", *c);
				exit(EXIT_FAILURE);
			}
			c++;
			e->else_branch = parse(1);
		} else if (*c == '(') {
			e->type = EXPR_CALL;
			e->op = *c;
			e->call = left;

			e->args = malloc(sizeof e->args[0]);
			e->len = 0;
			if (c[1] != ')') {
				do {
					c++;
					/* call with prec = 1 so that the commas are not recognized as binary operators */
					e->args[e->len++] = parse(1);
					e->args = realloc(e->args, (e->len + 1) * sizeof e->args[0]);
				} while (*c == ',');
			} else c++;

			if (*c != ')') {
				printf("(infix) expected ')', got '%c'\n", *c);
				exit(EXIT_FAILURE);
			}
			c++;
		} else if (infix_op->type == BINARY) {
			e->type = EXPR_BINARY;
			e->op = *c;
			e->left = left;
			c++;
			e->right = parse(infix_op->ass == LEFT ? infix_op->prec : infix_op->prec - 1);
		} else if (infix_op->type == POSTFIX) {
			e->type = EXPR_POSTFIX;
			e->op = *c;
			e->unary_operand = left;
			c++;
		}

		left = e;
	}

	return e;
}

void print(struct Expr *e)
{
	switch (e->type) {
	case EXPR_PREFIX: {
		if (e->op == '(') {
			print(e->unary_operand);
		} else {
			printf("(%c", e->op);
			print(e->unary_operand);
			printf(")");
		}
	} break;
	case EXPR_BINARY: {
		printf("(");
		print(e->left);
		printf(" %c ", e->op);
		print(e->right);
		printf(")");
	} break;
	case EXPR_VALUE: {
		printf("%c", e->val);
	} break;
	case EXPR_CALL: {
		printf("(");
		print(e->call);
		printf(" with <");
		for (size_t i = 0; i < e->len; i++) {
			print(e->args[i]);
			if (i != e->len - 1) printf(", ");
		}
		printf(">)");
	} break;
	case EXPR_TERNARY: {
		printf("(if ");
		print(e->condition);
		printf(" then ");
		print(e->then_branch);
		printf(" else ");
		print(e->else_branch);
		printf(")");
	} break;
	case EXPR_POSTFIX: {
		printf("(");
		print(e->unary_operand);
		printf(" %c)", e->op);
	} break;
	default: {
		printf("this should be unreachable.\n");
	}
	}
}

int indent = 0, levels[2048];

void p(struct Expr *e)
{
	printf("\n\t");
	levels[indent] = 0;

	for (int i = 0; i < indent - 1; i++) {
		if (levels[i]) printf("|   ");
		else printf("    ");
	}

	if (indent) {
		if (levels[indent - 1]) printf("|-- ");
		else printf("`-- ");
	}

	switch (e->type) {
	case EXPR_PREFIX: {
		if (e->op != '(') printf("(prefix %c)", e->op);
		else printf("(group)");
		indent++;
		p(e->unary_operand);
		indent--;
	} break;
	case EXPR_BINARY: {
		printf("(binary %c)", e->op);
		levels[indent] = 1;
		indent++;
		p(e->left);
		levels[indent - 1] = 0;
		p(e->right);
		indent--;
	} break;
	case EXPR_VALUE: {
		printf("(value %c)", e->val);
	} break;
	case EXPR_CALL: {
		printf("(function ");
		print(e->call);
		printf(")");
		levels[indent] = 1;
		indent++;
		for (size_t i = 0; i < e->len; i++) {
			if (i == e->len - 1) levels[indent - 1] = 0;
			p(e->args[i]);
		}
		indent--;
	} break;
	case EXPR_TERNARY: {
		printf("(ternary)");
		levels[indent] = 1;
		indent++;
		p(e->condition);
		p(e->then_branch);
		levels[indent - 1] = 0;
		p(e->else_branch);
		indent--;
	} break;
	case EXPR_POSTFIX: {
		printf("(postfix %c)", e->op);
		indent++;
		p(e->unary_operand);
		indent--;
	} break;
	default: {
		printf("this should be unreachable.\n");
	}
	}
}

int main(int argc, char **argv)
{
	printf("expression:\n\t%s\n", argv[1]);
	c = argv[1];
	struct Expr *e = parse(0);
	printf("syntax tree:");
	p(e);
	printf("\nparenthesized:\n\t");
	print(e);
	printf("\n\n");
	return 0;
}
