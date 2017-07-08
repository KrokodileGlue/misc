#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/*
class a {
	fn b (n) {
		var x;
	}

	fn c (n) {
		var x;
	}
}

a x;
x.b(5);
x.c(5);
*/

/*
C a {
	F b (n) {
		V x
	}

	F c (n) {
		V x
	}
}

U a b x;
U a c x;
*/

char sym_str[][32] = {
	"global",
	"class",
	"variable",
	"block",
	"function",
	"argument"
};

struct Sym {
	enum SymType {
		SYM_GLOBAL,
		SYM_CLASS,
		SYM_VAR,
		SYM_BLOCK,
		SYM_FN,
		SYM_ARG
	} type;

	char name;

	struct Sym *parent;
	struct Sym **children;
	int num;
};

struct Sym *symbol;

struct Stmt {
	enum {
		STMT_CLASS_DEF,
		STMT_FN_DEF,
		STMT_VAR_DEF,
		STMT_BLOCK,
		STMT_USE
	} type;

	union {
		struct {
			struct Stmt **body;
			int num;
			char *args;
			int num_args;
		};

		struct {
			char *names;
			int num;
		} use;
	};

	char name, instance;
};

void remove_whitespace(char *b)
{
	char *a = b;

	while (*b) {
		while (isspace(*b) && *b) b++;
		*a++ = *b;
		if (*b) b++;
	}

	*a = 0;
}

#define NEXT if (*a) a++;

#define EXPECT(EXPECT_CHAR)						\
	if (*a != EXPECT_CHAR)						\
		printf("unexpected token %c, expected %c\n", *a, EXPECT_CHAR); \
	NEXT;

char *a;

struct Stmt *parse()
{
	struct Stmt *s = malloc(sizeof *s);

	switch (*a) {
	case 'C':
		s->type = STMT_CLASS_DEF;
		NEXT; s->name = *a; NEXT;
		EXPECT('{');
		s->num = 0;
		s->body = NULL;

		while (*a != '}') {
			s->body = realloc(s->body, sizeof s->body[0] * (s->num + 1));
			s->body[s->num++] = parse(a);
		}

		EXPECT('}');
		break;
	case 'F':
		s->type = STMT_FN_DEF;

		NEXT; s->name = *a; NEXT;
		EXPECT('(');

		s->args = NULL;
		s->num_args = 0;

		while (*a != ')') {
			s->args = realloc(s->args, s->num_args + 1);
			s->args[s->num_args++] = *a;
			a++;
		}

		EXPECT(')');
		EXPECT('{');
		s->num = 0;
		s->body = NULL;

		while (*a != '}') {
			s->body = realloc(s->body, sizeof s->body[0] * (s->num + 1));
			s->body[s->num++] = parse(a);
		}

		EXPECT('}');
		break;
	case 'V':
		s->type = STMT_VAR_DEF;
		NEXT;
		s->name = *a;
		NEXT;
		break;
	case 'U':
		s->type = STMT_USE;
		s->use.num = 0;
		s->use.names = NULL;

		NEXT;
		while (*a != ';') {
			s->use.names = realloc(s->body, s->num + 1);
			s->use.names[s->use.num++] = *a;
			NEXT;
			if (*a == 0) break;
		}

		EXPECT(';');
		break;
	case '{':
		s->type = STMT_BLOCK;
		NEXT;

		while (*a != '}') {
			s->body = realloc(s->body, sizeof s->body * (s->num + 1));
			s->body[s->num++] = parse();
		}

		EXPECT('}');
		break;
	}

	return s;
}

int l = 0, arm[2048];

void indent()
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

void split()
{
	arm[l - 1] = 1;
}

void join()
{
	arm[l - 1] = 0;
}

void tree(struct Stmt *s)
{
	indent();

	switch (s->type) {
	case STMT_CLASS_DEF:
		printf("(class %c)", s->name);
		l++;
		split();
		for (int i = 0; i < s->num; i++) {
			if (i == s->num - 1) join();
			tree(s->body[i]);
		}
		l--;
		break;
	case STMT_FN_DEF:
		printf("(function %c)", s->name);
		l++;
		split();
		indent();

		printf("(args:");
		for (int i = 0; i < s->num_args; i++) {
			printf("%c", s->args[i]);
		}
		printf(")");
		join();

		indent();
		printf("(body)");
		l++;
		split();
		for (int i = 0; i < s->num; i++) {
			if (i == s->num - 1) join();
			tree(s->body[i]);
		}
		l -= 2;

		break;
	case STMT_VAR_DEF:
		printf("(variable %c)", s->name);
		break;
	case STMT_USE:
		printf("(use ");
		for (int i = 0; i < s->use.num; i++) {
			printf("%c", s->use.names[i]);
			if (i != s->use.num - 1) printf(".");
		}
		printf(")");
		break;
	case STMT_BLOCK:
		printf("(block)");
		l++; split();

		for (int i = 0; i < s->num; i++) {
			if (i == s->num - 1) join();
			tree(s->body[i]);
		}
		l--;
		break;
	}
}

struct Sym *resolve(char name, struct Sym *ctx)
{
	fprintf(stderr, "resolving symbol %c in context %c\n", name, ctx->name);
	struct Sym *s = NULL;

	while (1) {
		for (int i = 0; i < ctx->num; i++) {
			if (ctx->children[i]->name == name) {
				s = ctx->children[i];
			}
		}

		if (!s && ctx->parent) ctx = ctx->parent;
		else if (!s) return NULL;
		else return s;
	}
}

struct Sym *add(struct Sym *parent, char name, enum SymType type)
{
	struct Sym *s = malloc(sizeof *s);

	s->type = type;
	s->name = name;
	s->num = 0;
	s->children = NULL;
	s->parent = parent;

	parent->children = realloc(parent->children, sizeof parent->children[0] * (parent->num + 1));
	parent->children[parent->num++] = s;

	return s;
}

struct Sym *symbolize(struct Stmt *s)
{
	struct Sym *sym = malloc(sizeof *sym);
	sym->num = 0;
	sym->children = NULL;
	sym->name = s->name;
	sym->parent = symbol;
	symbol = sym;

	switch (s->type) {
	case STMT_CLASS_DEF:
		sym->type = SYM_CLASS;

		for (int i = 0; i < s->num; i++) {
			sym->children = realloc(sym->children, sizeof sym->children[0] * (sym->num + 1));
			struct Sym *child = symbolize(s->body[i]);
			if (child)
				sym->children[sym->num++] = symbolize(s->body[i]);
		}
		break;
	case STMT_FN_DEF:
		sym->type = SYM_FN;

		for (int i = 0; i < s->num_args; i++) {
			add(sym, s->args[i], SYM_ARG);
		}

		for (int i = 0; i < s->num; i++) {
			sym->children = realloc(sym->children, sizeof sym->children[0] * (sym->num + 1));
			struct Sym *child = symbolize(s->body[i]);
			if (child)
				sym->children[sym->num++] = symbolize(s->body[i]);
		}
		break;
	case STMT_VAR_DEF:
		sym->name = s->name;
		sym->type = SYM_VAR;
//		add(symbol, s->name, SYM_VAR);
		break;
	case STMT_USE: {
		struct Sym *ctx = sym->parent;

		for (int i = 0; i < s->use.num; i++) {
			ctx = resolve(s->use.names[i], ctx);
			if (!ctx) fprintf(stderr, "undeclared identifier %c\n", s->use.names[i]);
		}

		symbol = sym->parent;
		free(sym);
		return NULL;
	}
	case STMT_BLOCK:
		sym->name = '*';
		break;
	default:
		free(sym);
		return NULL;
	}

	symbol = sym->parent;
	return sym;
}

void symtree(struct Sym *s)
{
	indent();
	printf("[symbol %c : %s]", s->name, sym_str[s->type]);
	l++;
	split();
	for (int i = 0; i < s->num; i++) {
		if (i == s->num - 1) join();
		symtree(s->children[i]);
	}
	l--;
}

int main(int argc, char *argv[])
{
	remove_whitespace(argv[argc - 1]);
	printf("input: %s\n", argv[argc - 1]);

	printf("(root)");
	l++;
	split();

	a = argv[argc - 1];
	struct Stmt **s = NULL;
	int num = 0;

	while (*a) {
		s = realloc(s, sizeof s[0] * (num + 1));
		s[num++] = parse();
	}

	symbol = malloc(sizeof *symbol);
	symbol->type = SYM_GLOBAL;
	symbol->name = 'g';
	symbol->children = malloc(sizeof symbol->children[0] * num);
	symbol->parent = NULL;

	for (int i = 0; i < num; i++) {
		struct Sym *sym = symbolize(s[i]);
		if (sym) {
			symbol->children[symbol->num++] = sym;
		}
	}

	for (int i = 0; i < num; i++) {
		if (i == num - 1) join();
		tree(s[i]);
	}
	l--;

	printf("\n\nsymbol table:");
	symtree(symbol);

//	printf("finished parsing");
	printf("\n");
	return 0;
}
