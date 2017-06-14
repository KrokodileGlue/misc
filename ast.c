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
		PREFIX  = 1 << 0,
		INFIX   = 1 << 1,
		POSTFIX = 1 << 2
	} type;
} operators[] = {
	{ '(', 14, LEFT,  INFIX  }, /* for function call syntax */
	{ ',', 6,  RIGHT, INFIX  },
	{ '+', 12, LEFT,  PREFIX },
	{ '-', 11, LEFT,  PREFIX },
	{ '+', 7,  LEFT,  INFIX  },
	{ '-', 7,  LEFT,  INFIX  },
	{ '*', 9,  LEFT,  INFIX  },
	{ '/', 9,  LEFT,  INFIX  },
	{ '?', 5,  LEFT,  INFIX  },
	{ '=', 3,  LEFT,  INFIX  },
	{ '(', 2,  LEFT,  PREFIX },
};

struct Expr {
	enum ExprType {
		EXPR_NUMBER,
		EXPR_STR,
		EXPR_PREFIX,
		EXPR_BINARY,
		EXPR_POSTFIX,
		EXPR_MIXFIX,
		EXPR_CALL
	} type;
	char op;
	union {
		struct Expr *prefix_operand;
		struct {
			struct Expr *left, *right;
		};
		char str;
		char num;
		struct {
			struct Expr *condition, *then_branch, *else_branch;
		};
		struct {
			struct Expr *call, *arguments;
		};
	};
};

char *c;

int get_prec(int prec)
{
	for (size_t i = 0; i < sizeof operators / sizeof operators[0]; i++) {
		if (operators[i].body == *c && operators[i].type == INFIX) {
			return operators[i].prec;
		}
	}
	return prec;
}

struct Expr *parse(int prec)
{
	struct Op *prefix_op = NULL;
	for (size_t i = 0; i < sizeof operators / sizeof operators[0]; i++) {
		if (operators[i].body == *c && operators[i].type == PREFIX) {
			prefix_op = operators + i;
			break;
		}
	}

	struct Expr *left = malloc(sizeof *left);

	if (prefix_op) {
		left->type = EXPR_PREFIX;
		left->op = *c;
		c++;
		if (left->op == '(') {
			left->prefix_operand = parse(0);
			if (*c != ')') {
				printf("(prefix) expected ')', got '%c'\n", *c);
				exit(EXIT_FAILURE);
			}
			c++;
		} else {
			left->prefix_operand = parse(prefix_op->prec);
		}
	} else if (isalpha(*c)) {
		left->type = EXPR_STR;
		left->str = *c;
		c++;
	} else if (isdigit(*c)) {
		left->type = EXPR_NUMBER;
		left->num = *c;
		c++;
	} else {
		printf("oops, an error at '%c'.\n", *c);
		exit(EXIT_FAILURE);
	}

	struct Op *infix_op = NULL;
	struct Expr *e = left;

	while (prec < get_prec(prec)) {
		for (size_t i = 0; i < sizeof operators / sizeof operators[0]; i++) {
			if (operators[i].body == *c && operators[i].type == INFIX) {
				infix_op = operators + i;
			}
		}
		if (!infix_op) return left;

		e = malloc(sizeof *e);

		if (infix_op->body == '?') {
			e->type = EXPR_MIXFIX;
			e->op = *c;
			e->condition = left;
			c++;
			e->then_branch = parse(0);
			if (*c != ':') {
				printf("expected ':', got '%c'\n", *c);
				exit(EXIT_FAILURE);
			}
			c++;
			e->else_branch = parse(infix_op->prec);
		} else if (*c == '(') {
			e->type = EXPR_CALL;
			e->op = *c;
			e->call = left;
			c++;
			e->arguments = parse(0);
			if (*c != ')') {
				printf("(infix) expected ')', got '%c'\n", *c);
				exit(EXIT_FAILURE);
			}
			c++;
		} else {
			e->type = EXPR_BINARY;
			e->op = *c;
			e->left = left;
			c++;
			e->right = parse(infix_op->ass == LEFT ? infix_op->prec : infix_op->prec - 1);
		}

		left = e;
	}

	return e;
}

void print(struct Expr *e)
{
	if (e->type == EXPR_PREFIX) {
		if (e->op == '(') {
			print(e->prefix_operand);
		} else {
			printf("(%c", e->op);
			print(e->prefix_operand);
			printf(")");
		}
	} else if (e->type == EXPR_BINARY) {
		if (e->op != ',') printf("(");
		print(e->left);
		printf(" %c ", e->op);
		print(e->right);
		if (e->op != ',') printf(")");
	} else if (e->type == EXPR_STR) {
		printf("%c", e->str);
	} else if (e->type == EXPR_NUMBER) {
		printf("%c", e->num);
	} else if (e->type == EXPR_CALL) {
		printf("(");
		print(e->call);
		printf(" with (");
		print(e->arguments);
		printf("))");
	} else if (e->type == EXPR_MIXFIX) {
		printf("(if ");
		print(e->condition);
		printf(" then ");
		print(e->then_branch);
		printf(" else ");
		print(e->else_branch);
		printf(")");
	}
}

int main(int argc, char **argv)
{
	c = argv[1];
	print(parse(0));
	putchar('\n');
	return 0;
}
