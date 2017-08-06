#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

/*
 * a customized version of the FNV-1a hash function.
 * <http://www.isthe.com/chongo/tech/comp/fnv/index.html> 
 */

#define HASH_INIT ((0x84222325UL << sizeof (uint32_t)) | 0xcbf29ce4UL)

uint64_t
hash(const char *buf, size_t len, uint64_t hval)
{
	for (size_t i = 0; i < len; i++) {
		hval ^= (uint64_t)buf[i];
		hval *= (uint64_t)0x100000001b3ULL;
	}

	return hval;
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

#define NUM_TABLE_BUCKETS 32

struct table {
	struct bucket {
		uint64_t *key;
		struct value *val;
		size_t num;
	} bucket[NUM_TABLE_BUCKETS];
};

struct value *table_lookup(struct table *t, char *key);
struct value *table_add(struct table *t, char *key, struct value v);
struct table *new_table();

struct table *
new_table()
{
	struct table *t = malloc(sizeof *t);
	memset(t, 0, sizeof *t);
	return t;
}

struct value *
table_add(struct table *t, char *key, struct value v)
{
	uint64_t h = hash_str(key);
	struct value *o = table_lookup(t, key);
	if (o) return *o = v, o;

	int idx = h % NUM_TABLE_BUCKETS;
	struct bucket *b = t->bucket + idx;
	b->key  = realloc(b->key,  sizeof b->key[0]  * (b->num + 1));
	b->val  = realloc(b->val,  sizeof b->val[0]  * (b->num + 1));

	b->key [b->num] = h;
	b->val [b->num] = v;

	b->num++;
}

struct value *
table_lookup(struct table *t, char *key)
{
	uint64_t h = hash_str(key);
	int idx = h % NUM_TABLE_BUCKETS;

	struct bucket *b = t->bucket + idx;

	for (int i = 0; i < b->num; i++) {
		if (b->key[i] == h) {
			return b->val + i;
		}
	}

	return NULL;
}

int
main(int argc, char **argv)
{
	struct table *t = new_table();

	table_add(t, "foo", (struct value){ VAL_STR, { .string  = "test" } });
	table_add(t, "bar", (struct value){ VAL_INT, { .integer = 256 } });
	table_add(t, "baz", (struct value){ VAL_STR, { .string  = "deadbeef" } });

	print_value(table_lookup(t, "foo"));
	print_value(table_lookup(t, "bar"));
	print_value(table_lookup(t, "baz"));

	print_value(table_lookup(t, "nonexistent"));

	return EXIT_SUCCESS;
}
