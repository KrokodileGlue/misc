#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <stdbool.h>

/*
 * The FNV-1a hash function.
 * <http://www.isthe.com/chongo/tech/comp/fnv/index.html> 
 */

#define HASH_INIT ((0x84222325UL << sizeof (uint32_t)) | 0xcbf29ce4UL)
#define FNV_PRIME ((uint64_t)0x100000001b3)

uint64_t
hash(const char *buf, size_t len, uint64_t h)
{
	for (size_t i = 0; i < len; i++) {
		h ^= (uint64_t)buf[i];
		h *= FNV_PRIME;
	}

	return h;
}

uint64_t
hash_str(const char *s)
{
	return hash(s, strlen(s), HASH_INIT);
}

uint64_t
hash_int(int i)
{
	return hash((char *)&i, sizeof i, HASH_INIT);
}

struct value {
	enum {
		VAL_INT,
		VAL_STR
	} type;

	union {
		int integer;
		char *string;
	} d;
};

uint64_t
hash_value(const struct value v)
{
	switch (v.type) {
	case VAL_INT: return hash_int(v.d.integer);
	case VAL_STR: return hash_str(v.d.string);
	}
}

void
print_value(struct value *v)
{
	if (!v) {
		printf("(nil)\n");
		return;
	}

	switch (v->type) {
	case VAL_INT: printf("%d\n", v->d.integer); break;
	case VAL_STR: printf("%s\n", v->d.string); break;
	}
}

bool
val_cmp(struct value l, struct value r)
{
	if (l.type != r.type) return false;

	switch (l.type) {
	case VAL_INT: return l.d.integer == r.d.integer; break;
	case VAL_STR: return strcmp(l.d.string, r.d.string) == 0; break;
	}
}

#define TABLE_SIZE 32

struct table {
	struct bucket {
		uint64_t *h;
		struct value *key;
		struct value *val;
		size_t len;
	} bucket[TABLE_SIZE];
};

struct value *table_lookup(struct table *t, struct value key);
struct value *table_add(struct table *t, struct value key, struct value v);
struct table *new_table();

struct table *
new_table()
{
	struct table *t = malloc(sizeof *t);
	memset(t, 0, sizeof *t);
	return t;
}

void
free_table(struct table *t)
{
	for (size_t i = 0; i < TABLE_SIZE; i++) {
		if (t->bucket[i].h)   free(t->bucket[i].h);
		if (t->bucket[i].key) free(t->bucket[i].key);
		if (t->bucket[i].val) free(t->bucket[i].val);
	}

	free(t);
}

struct value *
table_add(struct table *t, struct value key, struct value v)
{
	uint64_t h = hash_value(key);
	struct value *o = table_lookup(t, key);
	if (o) return *o = v, o;

	int idx = h % TABLE_SIZE;
	struct bucket *b = t->bucket + idx;
	b->h   = realloc(b->h,   sizeof b->h[0]   * (b->len + 1));
	b->val = realloc(b->val, sizeof b->val[0] * (b->len + 1));
	b->key = realloc(b->key, sizeof b->key[0] * (b->len + 1));

	b->h  [b->len] = h;
	b->val[b->len] = v;
	b->key[b->len] = key;

	b->len++;
}

struct value *
table_lookup(struct table *t, struct value key)
{
	uint64_t h = hash_value(key);
	int idx = h % TABLE_SIZE;

	struct bucket *b = t->bucket + idx;

	for (int i = 0; i < b->len; i++) {
		if (b->h[i] == h && val_cmp(b->key[i], key)) {
			return b->val + i;
		}
	}

	return NULL;
}

#define STR(x) ((struct value){ VAL_STR, { .string  = x } })
#define INT(x) ((struct value){ VAL_INT, { .integer  = x } })

int
main(int argc, char **argv)
{
	struct table *t = new_table();

	table_add(t, STR("foo"),      STR("test"));
	table_add(t, STR("bar"),      INT(256));
	table_add(t, STR("baz"),      STR("deadbeef"));

	print_value(table_lookup(t, STR("foo")));
	print_value(table_lookup(t, STR("bar")));
	print_value(table_lookup(t, STR("baz")));

	print_value(table_lookup(t, STR("whatever")));

	free_table(t);

	return EXIT_SUCCESS;
}
